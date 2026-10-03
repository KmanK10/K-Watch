// NimBLE peripheral that acts as a GATT client to the iPhone's ANCS, AMS and CTS.
//
// After the link is encrypted, setup runs as a chain of GATT operations:
// find services -> find characteristics -> find CCCDs -> subscribe -> register.
// Each step's completion callback starts the next one.
//
// After setup, every write to the phone goes through one queue so only one ATT
// request is ever outstanding.

#include "phone.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

void ble_store_config_init(void);

static const char *TAG = "phone";

// Apple service UUIDs, bytes reversed.
// ANCS 7905F431-B5CE-4E99-A40F-4B1E122D00D0
static const ble_uuid128_t ANCS_SVC = BLE_UUID128_INIT(
    0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79);
static const ble_uuid128_t ANCS_NOTIF_SRC = BLE_UUID128_INIT(
    0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C, 0xD9, 0x42, 0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F);
static const ble_uuid128_t ANCS_CTRL_PT = BLE_UUID128_INIT(
    0xD9, 0xD9, 0xAA, 0xFD, 0xBD, 0x9B, 0x21, 0x98, 0xA8, 0x49, 0xE1, 0x45, 0xF3, 0xD8, 0xD1, 0x69);
static const ble_uuid128_t ANCS_DATA_SRC = BLE_UUID128_INIT(
    0xFB, 0x7B, 0x7C, 0xCE, 0x6A, 0xB3, 0x44, 0xBE, 0xB5, 0x4B, 0xD6, 0x24, 0xE9, 0xC6, 0xEA, 0x22);
// AMS 89D3502B-0F36-433A-8EF4-C502AD55F8DC
static const ble_uuid128_t AMS_SVC = BLE_UUID128_INIT(
    0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89);
static const ble_uuid128_t AMS_REMOTE_CMD = BLE_UUID128_INIT(
    0xC2, 0x51, 0xCA, 0xF7, 0x56, 0x0E, 0xDF, 0xB8, 0x8A, 0x4A, 0xB1, 0x57, 0xD8, 0x81, 0x3C, 0x9B);
static const ble_uuid128_t AMS_ENTITY_UPDATE = BLE_UUID128_INIT(
    0x02, 0xC1, 0x96, 0xBA, 0x92, 0xBB, 0x0C, 0x9A, 0x1F, 0x41, 0x8D, 0x80, 0xCE, 0xAB, 0x7C, 0x2F);
static const ble_uuid16_t CTS_SVC = BLE_UUID16_INIT(0x1805);
static const ble_uuid16_t CTS_CURRENT_TIME = BLE_UUID16_INIT(0x2A2B);
static const ble_uuid16_t CCCD = BLE_UUID16_INIT(BLE_GATT_DSC_CLT_CFG_UUID16);

// ANCS protocol values
#define EVT_ADDED              0
#define EVT_REMOVED            2
#define FLAG_SILENT            (1 << 0)
#define FLAG_PRE_EXISTING      (1 << 2)
#define FLAG_POSITIVE_ACTION   (1 << 3)
#define FLAG_NEGATIVE_ACTION   (1 << 4)
#define CMD_GET_NOTIF_ATTRS    0
#define CMD_GET_APP_ATTRS      1
#define CMD_PERFORM_ACTION     2
#define APP_ATTR_DISPLAY_NAME  0
#define ATTR_APP_ID            0
#define ATTR_TITLE             1
#define ATTR_SUBTITLE          2
#define ATTR_MESSAGE           3
#define ATTR_COUNT             4

// AMS protocol values
#define ENTITY_PLAYER          0
#define ENTITY_TRACK           2
#define PLAYER_PLAYBACK_INFO   1
#define PLAYER_VOLUME          2
#define TRACK_ARTIST           0
#define TRACK_TITLE            2

#define ADV_FAST_MS            30000
#define MAX_CHRS               8
#define PENDING_MAX            16
#define DATA_BUF_SIZE          512
#define WRITE_QUEUE_LEN        8
#define CMD_QUEUE_LEN          8
#define APP_CACHE_LEN          16

typedef enum {
    STEP_ANCS_SVC,
    STEP_ANCS_CHRS,
    STEP_ANCS_DSCS,
    STEP_SUB_DATA_SRC,
    STEP_SUB_NOTIF_SRC,
    STEP_CTS_SVC,
    STEP_CTS_CHRS,
    STEP_CTS_DSCS,
    STEP_SUB_TIME,
    STEP_READ_TIME,
    STEP_AMS_SVC,
    STEP_AMS_CHRS,
    STEP_AMS_DSCS,
    STEP_SUB_REMOTE_CMD,
    STEP_SUB_ENTITY,
    STEP_REGISTER_PLAYER,
    STEP_REGISTER_TRACK,
    STEP_DONE,
} setup_step_t;

typedef struct {
    setup_step_t after_svc;
    setup_step_t after_chrs;
    setup_step_t after_dscs;
    uint16_t start;
    uint16_t end;
    uint16_t chr_vals[MAX_CHRS];   // every characteristic, to tell which one owns a CCCD
    int chr_count;
} service_t;

typedef struct {
    uint32_t uid;
    uint8_t category;
    uint8_t flags;
} pending_t;

typedef struct {
    uint16_t handle;
    uint8_t len;
    uint8_t data[64];
    bool is_ancs_request;
} write_op_t;

typedef enum { REQ_NOTIFICATION, REQ_APP_NAME } request_kind_t;

typedef struct {
    char app_id[48];
    char name[32];
} app_name_t;

typedef enum { CMD_MEDIA, CMD_NOTIF_ACTION, CMD_LINK_SPEED, CMD_FORGET, CMD_ENABLE } cmd_kind_t;

typedef struct {
    cmd_kind_t kind;
    uint8_t arg;
    uint32_t uid;
} phone_cmd_t;

static QueueHandle_t s_events;
static QueueHandle_t s_cmds;
static struct ble_npl_event s_cmd_event;
static uint8_t s_own_addr_type;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static bool s_setup_done;
static bool s_backlog_done;
static volatile bool s_enabled = true;
static volatile bool s_interactive;
static bool s_params_pending;
static int s_params_requested;
static int s_params_applied = -1;   // 1 fast, 0 slow, -1 unknown

static service_t s_ancs, s_cts, s_ams;
static uint16_t s_notif_src, s_notif_src_cccd;
static uint16_t s_ctrl_pt;
static uint16_t s_data_src, s_data_src_cccd;
static uint16_t s_time, s_time_cccd;
static uint16_t s_remote_cmd, s_remote_cmd_cccd;
static uint16_t s_entity, s_entity_cccd;

static pending_t s_pending[PENDING_MAX];
static int s_pending_count;
static bool s_request_active;
static request_kind_t s_request_kind;
static phone_event_t s_ready;   // notification being fetched, waiting for its app name
static app_name_t s_app_names[APP_CACHE_LEN];
static int s_app_next;
static uint8_t s_data[DATA_BUF_SIZE];
static size_t s_data_len;

static write_op_t s_writes[WRITE_QUEUE_LEN];
static int s_write_count;
static bool s_write_busy;

static phone_media_t s_media;

static void run_step(setup_step_t step);
static void advertise(bool fast);

static void post(const phone_event_t *evt)
{
    if (xQueueSend(s_events, evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropped event %d", evt->type);
    }
}

static void post_simple(phone_event_type_t type)
{
    phone_event_t evt = {.type = type};
    post(&evt);
}

static void reset_service(service_t *s, setup_step_t after_svc, setup_step_t after_chrs,
                          setup_step_t after_dscs)
{
    memset(s, 0, sizeof(*s));
    s->after_svc = after_svc;
    s->after_chrs = after_chrs;
    s->after_dscs = after_dscs;
}

static void reset_link_state(void)
{
    reset_service(&s_ancs, STEP_ANCS_CHRS, STEP_ANCS_DSCS, STEP_SUB_DATA_SRC);
    reset_service(&s_cts, STEP_CTS_CHRS, STEP_CTS_DSCS, STEP_SUB_TIME);
    reset_service(&s_ams, STEP_AMS_CHRS, STEP_AMS_DSCS, STEP_SUB_REMOTE_CMD);
    s_notif_src = s_notif_src_cccd = s_ctrl_pt = 0;
    s_data_src = s_data_src_cccd = s_time = s_time_cccd = 0;
    s_remote_cmd = s_remote_cmd_cccd = s_entity = s_entity_cccd = 0;
    s_setup_done = false;
    s_backlog_done = false;
    s_params_pending = false;
    s_params_applied = -1;
    s_pending_count = 0;
    s_request_active = false;
    s_data_len = 0;
    s_write_count = 0;
    s_write_busy = false;
    memset(&s_media, 0, sizeof(s_media));
}

// ---- Write queue (used once setup is done) ----

static void fail_request(void);

static void write_pump(void);

static int on_queued_write(uint16_t conn, const struct ble_gatt_error *err,
                           struct ble_gatt_attr *attr, void *arg)
{
    (void)conn;
    (void)attr;
    (void)arg;
    bool was_ancs_request = s_writes[0].is_ancs_request;
    if (err->status != 0) {
        ESP_LOGW(TAG, "write to 0x%04x failed: %d", s_writes[0].handle, err->status);
    }
    memmove(&s_writes[0], &s_writes[1], (s_write_count - 1) * sizeof(s_writes[0]));
    s_write_count--;
    s_write_busy = false;

    // A failed details request usually means the notification is already gone.
    if (was_ancs_request && err->status != 0) {
        fail_request();
    }
    write_pump();
    return 0;
}

static void write_pump(void)
{
    if (s_write_busy || s_write_count == 0 || s_conn == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    s_write_busy = true;
    const write_op_t *w = &s_writes[0];
    if (ble_gattc_write_flat(s_conn, w->handle, w->data, w->len, on_queued_write, NULL) != 0) {
        struct ble_gatt_error err = {.status = BLE_HS_EUNKNOWN};
        on_queued_write(s_conn, &err, NULL, NULL);
    }
}

static bool write_enqueue(uint16_t handle, const uint8_t *data, uint8_t len, bool is_ancs_request)
{
    if (!handle || s_write_count >= WRITE_QUEUE_LEN || len > sizeof(s_writes[0].data)) {
        return false;
    }
    write_op_t *w = &s_writes[s_write_count++];
    w->handle = handle;
    w->len = len;
    w->is_ancs_request = is_ancs_request;
    memcpy(w->data, data, len);
    write_pump();
    return true;
}

// ---- Notification details (ANCS Control Point -> Data Source) ----

static void request_next_notification(void);

static void finish_request(void)
{
    if (s_pending_count > 0) {
        memmove(&s_pending[0], &s_pending[1], (s_pending_count - 1) * sizeof(s_pending[0]));
        s_pending_count--;
    }
    s_request_active = false;
    s_data_len = 0;
    request_next_notification();
}

static void deliver_ready(void)
{
    const phone_notification_t *n = &s_ready.notification;
    ESP_LOGI(TAG, "notification %lu from %s (%s), category %d%s%s%s%s", (unsigned long)n->uid,
             n->app_name[0] ? n->app_name : n->app_id, n->app_id, n->category,
             n->pre_existing ? " (already on phone)" : "", n->silent ? " (silent)" : "",
             n->has_positive ? " +action" : "", n->has_negative ? " -action" : "");
    post(&s_ready);
    finish_request();
}

// If only the app name lookup failed, the notification is still worth showing.
static void fail_request(void)
{
    if (s_request_active && s_request_kind == REQ_APP_NAME) {
        deliver_ready();
    } else {
        finish_request();
    }
}

static const char *cached_app_name(const char *app_id)
{
    for (int i = 0; i < APP_CACHE_LEN; i++) {
        if (strcmp(s_app_names[i].app_id, app_id) == 0) {
            return s_app_names[i].name;
        }
    }
    return NULL;
}

static void cache_app_name(const char *app_id, const char *name)
{
    app_name_t *slot = &s_app_names[s_app_next];
    s_app_next = (s_app_next + 1) % APP_CACHE_LEN;
    strlcpy(slot->app_id, app_id, sizeof(slot->app_id));
    strlcpy(slot->name, name, sizeof(slot->name));
}

static void request_app_name(void)
{
    const char *app_id = s_ready.notification.app_id;
    size_t len = strlen(app_id);
    uint8_t cmd[2 + sizeof(s_ready.notification.app_id)];
    cmd[0] = CMD_GET_APP_ATTRS;
    memcpy(&cmd[1], app_id, len + 1);
    cmd[len + 2] = APP_ATTR_DISPLAY_NAME;

    s_request_kind = REQ_APP_NAME;
    s_data_len = 0;
    if (!write_enqueue(s_ctrl_pt, cmd, len + 3, true)) {
        deliver_ready();
    }
}

static void apply_link_params(void);

static void request_next_notification(void)
{
    if (!s_setup_done || s_request_active) {
        return;
    }
    if (s_pending_count == 0) {
        // Stay on the fast connection until the backlog is fetched.
        if (!s_backlog_done) {
            s_backlog_done = true;
            apply_link_params();
        }
        return;
    }
    uint32_t uid = s_pending[0].uid;
    const uint8_t cmd[] = {
        CMD_GET_NOTIF_ATTRS,
        uid & 0xFF, (uid >> 8) & 0xFF, (uid >> 16) & 0xFF, uid >> 24,
        ATTR_APP_ID,
        ATTR_TITLE, sizeof(((phone_notification_t *)0)->title) - 1, 0,
        ATTR_SUBTITLE, sizeof(((phone_notification_t *)0)->subtitle) - 1, 0,
        ATTR_MESSAGE, sizeof(((phone_notification_t *)0)->message) - 1, 0,
    };
    s_request_active = true;
    s_request_kind = REQ_NOTIFICATION;
    s_data_len = 0;
    if (!write_enqueue(s_ctrl_pt, cmd, sizeof(cmd), true)) {
        finish_request();
    }
}

static void copy_attr(char *dst, size_t dst_size, const uint8_t *src, size_t len)
{
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

// The response can arrive split over several packets. Returns true once all of it is here.
static bool parse_data_source(phone_notification_t *n)
{
    if (s_data_len < 5 || s_data[0] != CMD_GET_NOTIF_ATTRS) {
        return false;
    }
    size_t pos = 5;
    for (int i = 0; i < ATTR_COUNT; i++) {
        if (pos + 3 > s_data_len) {
            return false;
        }
        uint8_t id = s_data[pos];
        size_t len = s_data[pos + 1] | (s_data[pos + 2] << 8);
        if (pos + 3 + len > s_data_len) {
            return false;
        }
        const uint8_t *val = &s_data[pos + 3];
        if (id == ATTR_APP_ID) {
            copy_attr(n->app_id, sizeof(n->app_id), val, len);
        } else if (id == ATTR_TITLE) {
            copy_attr(n->title, sizeof(n->title), val, len);
        } else if (id == ATTR_SUBTITLE) {
            copy_attr(n->subtitle, sizeof(n->subtitle), val, len);
        } else if (id == ATTR_MESSAGE) {
            copy_attr(n->message, sizeof(n->message), val, len);
        }
        pos += 3 + len;
    }
    n->uid = s_data[1] | (s_data[2] << 8) | (s_data[3] << 16) | ((uint32_t)s_data[4] << 24);
    return true;
}

// Response to Get App Attributes: command, app ID (NUL-terminated), then one attribute.
static bool parse_app_name(char *out, size_t out_size)
{
    if (s_data_len < 2 || s_data[0] != CMD_GET_APP_ATTRS) {
        return false;
    }
    const uint8_t *nul = memchr(&s_data[1], '\0', s_data_len - 1);
    if (!nul) {
        return false;
    }
    size_t pos = (nul - s_data) + 1;
    if (pos + 3 > s_data_len) {
        return false;
    }
    size_t len = s_data[pos + 1] | (s_data[pos + 2] << 8);
    if (pos + 3 + len > s_data_len) {
        return false;
    }
    copy_attr(out, out_size, &s_data[pos + 3], len);
    return true;
}

static void on_data_source(const uint8_t *data, size_t len)
{
    if (!s_request_active) {
        return;
    }
    if (s_data_len + len > sizeof(s_data)) {
        ESP_LOGW(TAG, "notification too large, skipped");
        fail_request();
        return;
    }
    memcpy(&s_data[s_data_len], data, len);
    s_data_len += len;

    phone_notification_t *n = &s_ready.notification;
    if (s_request_kind == REQ_APP_NAME) {
        if (parse_app_name(n->app_name, sizeof(n->app_name))) {
            cache_app_name(n->app_id, n->app_name);
            deliver_ready();
        }
        return;
    }

    memset(&s_ready, 0, sizeof(s_ready));
    s_ready.type = PHONE_EVT_NOTIFICATION;
    if (!parse_data_source(n)) {
        return;
    }
    const pending_t *p = &s_pending[0];
    n->category = p->category;
    n->pre_existing = p->flags & FLAG_PRE_EXISTING;
    n->silent = p->flags & FLAG_SILENT;
    n->has_positive = p->flags & FLAG_POSITIVE_ACTION;
    n->has_negative = p->flags & FLAG_NEGATIVE_ACTION;

    const char *name = cached_app_name(n->app_id);
    if (name || n->app_id[0] == '\0') {
        strlcpy(n->app_name, name ? name : "", sizeof(n->app_name));
        deliver_ready();
    } else {
        request_app_name();
    }
}

static void on_notification_source(const uint8_t *data, size_t len)
{
    if (len < 8) {
        return;
    }
    uint8_t event_id = data[0];
    uint8_t flags = data[1];
    uint8_t category = data[2];
    uint32_t uid = data[4] | (data[5] << 8) | (data[6] << 16) | ((uint32_t)data[7] << 24);

    if (event_id == EVT_ADDED) {
        // When full, forget the oldest one that is not already being fetched.
        if (s_pending_count == PENDING_MAX) {
            int drop = s_request_active ? 1 : 0;
            memmove(&s_pending[drop], &s_pending[drop + 1],
                    (PENDING_MAX - drop - 1) * sizeof(s_pending[0]));
            s_pending_count--;
        }
        s_pending[s_pending_count++] = (pending_t){.uid = uid, .category = category, .flags = flags};
        request_next_notification();
    } else if (event_id == EVT_REMOVED) {
        ESP_LOGI(TAG, "notification %lu removed", (unsigned long)uid);
        // No point asking for details of one that is gone; leave the in-flight one alone.
        for (int i = s_request_active ? 1 : 0; i < s_pending_count; i++) {
            if (s_pending[i].uid == uid) {
                memmove(&s_pending[i], &s_pending[i + 1],
                        (s_pending_count - i - 1) * sizeof(s_pending[0]));
                s_pending_count--;
                break;
            }
        }
        phone_event_t evt = {.type = PHONE_EVT_NOTIFICATION_REMOVED, .uid = uid};
        post(&evt);
    }
}

// ---- Media (AMS Entity Update) ----

static void on_entity_update(const uint8_t *data, size_t len)
{
    if (len < 3) {
        return;
    }
    uint8_t entity = data[0];
    uint8_t attr = data[1];
    char value[64];
    copy_attr(value, sizeof(value), &data[3], len - 3);

    if (entity == ENTITY_PLAYER && attr == PLAYER_PLAYBACK_INFO) {
        // "state,rate,elapsed": state 0 is paused, 1 playing, 2/3 seeking.
        s_media.playing = value[0] == '1' || value[0] == '2' || value[0] == '3';
    } else if (entity == ENTITY_PLAYER && attr == PLAYER_VOLUME) {
        s_media.volume = (uint8_t)(strtof(value, NULL) * 100 + 0.5f);
    } else if (entity == ENTITY_TRACK && attr == TRACK_TITLE) {
        copy_attr(s_media.title, sizeof(s_media.title), &data[3], len - 3);
    } else if (entity == ENTITY_TRACK && attr == TRACK_ARTIST) {
        copy_attr(s_media.artist, sizeof(s_media.artist), &data[3], len - 3);
    } else {
        return;
    }
    phone_event_t evt = {.type = PHONE_EVT_MEDIA, .media = s_media};
    post(&evt);
}

// ---- Time (CTS Current Time) ----

static void on_time_value(const uint8_t *d, size_t len)
{
    if (len < 7) {
        return;
    }
    phone_event_t evt = {.type = PHONE_EVT_TIME};
    evt.time.tm_year = (d[0] | (d[1] << 8)) - 1900;
    evt.time.tm_mon = d[2] - 1;
    evt.time.tm_mday = d[3];
    evt.time.tm_hour = d[4];
    evt.time.tm_min = d[5];
    evt.time.tm_sec = d[6];
    evt.time.tm_isdst = -1;
    post(&evt);
}

static int on_time_read(uint16_t conn, const struct ble_gatt_error *err,
                        struct ble_gatt_attr *attr, void *arg)
{
    (void)conn;
    (void)arg;
    if (err->status == 0 && attr) {
        uint8_t buf[10];
        uint16_t len = 0;
        if (ble_hs_mbuf_to_flat(attr->om, buf, sizeof(buf), &len) == 0) {
            on_time_value(buf, len);
        }
    }
    run_step(STEP_AMS_SVC);
    return 0;
}

// ---- Setup chain ----

static int on_svc(uint16_t conn, const struct ble_gatt_error *err,
                  const struct ble_gatt_svc *svc, void *arg)
{
    (void)conn;
    service_t *s = arg;
    if (err->status == 0) {
        s->start = svc->start_handle;
        s->end = svc->end_handle;
        return 0;
    }
    run_step(s->after_svc);
    return 0;
}

static void match_chr(const service_t *s, const struct ble_gatt_chr *chr)
{
    const ble_uuid_t *u = &chr->uuid.u;
    if (s == &s_ancs) {
        if (ble_uuid_cmp(u, &ANCS_NOTIF_SRC.u) == 0) {
            s_notif_src = chr->val_handle;
        } else if (ble_uuid_cmp(u, &ANCS_CTRL_PT.u) == 0) {
            s_ctrl_pt = chr->val_handle;
        } else if (ble_uuid_cmp(u, &ANCS_DATA_SRC.u) == 0) {
            s_data_src = chr->val_handle;
        }
    } else if (s == &s_cts) {
        if (ble_uuid_cmp(u, &CTS_CURRENT_TIME.u) == 0) {
            s_time = chr->val_handle;
        }
    } else if (s == &s_ams) {
        if (ble_uuid_cmp(u, &AMS_REMOTE_CMD.u) == 0) {
            s_remote_cmd = chr->val_handle;
        } else if (ble_uuid_cmp(u, &AMS_ENTITY_UPDATE.u) == 0) {
            s_entity = chr->val_handle;
        }
    }
}

static int on_chr(uint16_t conn, const struct ble_gatt_error *err,
                  const struct ble_gatt_chr *chr, void *arg)
{
    (void)conn;
    service_t *s = arg;
    if (err->status == 0) {
        if (s->chr_count < MAX_CHRS) {
            s->chr_vals[s->chr_count++] = chr->val_handle;
        }
        match_chr(s, chr);
        return 0;
    }
    run_step(s->after_chrs);
    return 0;
}

// A descriptor belongs to the closest characteristic value before it.
static uint16_t owning_chr(const service_t *s, uint16_t dsc_handle)
{
    uint16_t best = 0;
    for (int i = 0; i < s->chr_count; i++) {
        if (s->chr_vals[i] < dsc_handle && s->chr_vals[i] > best) {
            best = s->chr_vals[i];
        }
    }
    return best;
}

static int on_dsc(uint16_t conn, const struct ble_gatt_error *err, uint16_t chr_val_handle,
                  const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)conn;
    (void)chr_val_handle;
    service_t *s = arg;
    if (err->status == 0) {
        if (ble_uuid_cmp(&dsc->uuid.u, &CCCD.u) == 0) {
            uint16_t owner = owning_chr(s, dsc->handle);
            uint16_t *const pairs[][2] = {
                {&s_notif_src, &s_notif_src_cccd},
                {&s_data_src, &s_data_src_cccd},
                {&s_time, &s_time_cccd},
                {&s_remote_cmd, &s_remote_cmd_cccd},
                {&s_entity, &s_entity_cccd},
            };
            for (size_t i = 0; owner && i < sizeof(pairs) / sizeof(pairs[0]); i++) {
                if (*pairs[i][0] == owner) {
                    *pairs[i][1] = dsc->handle;
                }
            }
        }
        return 0;
    }
    run_step(s->after_dscs);
    return 0;
}

static int on_setup_write(uint16_t conn, const struct ble_gatt_error *err,
                          struct ble_gatt_attr *attr, void *arg)
{
    (void)conn;
    (void)attr;
    if (err->status != 0) {
        ESP_LOGW(TAG, "setup write failed: %d", err->status);
    }
    run_step((setup_step_t)(uintptr_t)arg);
    return 0;
}

// Starts a write that continues with `next`. If it cannot start, continues right away.
static void setup_write(uint16_t handle, const uint8_t *data, uint16_t len, setup_step_t next)
{
    if (!handle || ble_gattc_write_flat(s_conn, handle, data, len, on_setup_write,
                                        (void *)(uintptr_t)next) != 0) {
        run_step(next);
    }
}

static void subscribe(uint16_t cccd, setup_step_t next)
{
    static const uint8_t enable[2] = {0x01, 0x00};
    setup_write(cccd, enable, sizeof(enable), next);
}

static void discover_service(service_t *s, const ble_uuid_t *uuid)
{
    if (ble_gattc_disc_svc_by_uuid(s_conn, uuid, on_svc, s) != 0) {
        run_step(s->after_svc);
    }
}

static void discover_chrs(service_t *s, setup_step_t skip_to)
{
    if (!s->start || ble_gattc_disc_all_chrs(s_conn, s->start, s->end, on_chr, s) != 0) {
        run_step(skip_to);
    }
}

static void discover_dscs(service_t *s)
{
    if (ble_gattc_disc_all_dscs(s_conn, s->start, s->end, on_dsc, s) != 0) {
        run_step(s->after_dscs);
    }
}

// Fast while the screen is on so buttons feel instant, slow otherwise to save power.
// Both stay within Apple's accessory guidelines: max >= min + 15ms, max * (latency + 1) <= 2s.
static void apply_link_params(void)
{
    static const struct ble_gap_upd_params fast = {
        .itvl_min = BLE_GAP_CONN_ITVL_MS(15),
        .itvl_max = BLE_GAP_CONN_ITVL_MS(30),
        .latency = 0,
        .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(4000),
    };
    static const struct ble_gap_upd_params slow = {
        .itvl_min = BLE_GAP_CONN_ITVL_MS(150),
        .itvl_max = BLE_GAP_CONN_ITVL_MS(180),
        .latency = 4,
        .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(6000),
    };
    // Overlapping update requests can go unanswered and drop the link, so only one at a time.
    if (s_conn == BLE_HS_CONN_HANDLE_NONE || !s_backlog_done || s_params_pending) {
        return;
    }
    int want = s_interactive ? 1 : 0;
    if (want == s_params_applied) {
        return;
    }
    struct ble_gap_upd_params p = want ? fast : slow;
    if (ble_gap_update_params(s_conn, &p) == 0) {
        s_params_pending = true;
        s_params_requested = want;
    }
}

static void run_step(setup_step_t step)
{
    if (s_conn == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    switch (step) {
    case STEP_ANCS_SVC:
        discover_service(&s_ancs, &ANCS_SVC.u);
        break;
    case STEP_ANCS_CHRS:
        discover_chrs(&s_ancs, STEP_CTS_SVC);
        break;
    case STEP_ANCS_DSCS:
        discover_dscs(&s_ancs);
        break;
    case STEP_SUB_DATA_SRC:
        // Apple asks for Data Source to be subscribed before Notification Source.
        subscribe(s_data_src_cccd, STEP_SUB_NOTIF_SRC);
        break;
    case STEP_SUB_NOTIF_SRC:
        subscribe(s_notif_src_cccd, STEP_CTS_SVC);
        break;
    case STEP_CTS_SVC:
        discover_service(&s_cts, &CTS_SVC.u);
        break;
    case STEP_CTS_CHRS:
        discover_chrs(&s_cts, STEP_AMS_SVC);
        break;
    case STEP_CTS_DSCS:
        discover_dscs(&s_cts);
        break;
    case STEP_SUB_TIME:
        subscribe(s_time_cccd, STEP_READ_TIME);
        break;
    case STEP_READ_TIME:
        if (!s_time || ble_gattc_read(s_conn, s_time, on_time_read, NULL) != 0) {
            run_step(STEP_AMS_SVC);
        }
        break;
    case STEP_AMS_SVC:
        discover_service(&s_ams, &AMS_SVC.u);
        break;
    case STEP_AMS_CHRS:
        discover_chrs(&s_ams, STEP_DONE);
        break;
    case STEP_AMS_DSCS:
        discover_dscs(&s_ams);
        break;
    case STEP_SUB_REMOTE_CMD:
        subscribe(s_remote_cmd_cccd, STEP_SUB_ENTITY);
        break;
    case STEP_SUB_ENTITY:
        subscribe(s_entity_cccd, STEP_REGISTER_PLAYER);
        break;
    case STEP_REGISTER_PLAYER: {
        static const uint8_t reg[] = {ENTITY_PLAYER, PLAYER_PLAYBACK_INFO, PLAYER_VOLUME};
        setup_write(s_entity_cccd ? s_entity : 0, reg, sizeof(reg), STEP_REGISTER_TRACK);
        break;
    }
    case STEP_REGISTER_TRACK: {
        static const uint8_t reg[] = {ENTITY_TRACK, TRACK_ARTIST, TRACK_TITLE};
        setup_write(s_entity_cccd ? s_entity : 0, reg, sizeof(reg), STEP_DONE);
        break;
    }
    case STEP_DONE:
        ESP_LOGI(TAG, "setup done: notifications %s, time %s, media %s",
                 s_notif_src_cccd ? "yes" : "no", s_time ? "yes" : "no",
                 s_entity_cccd ? "yes" : "no");
        s_setup_done = true;
        request_next_notification();
        break;
    }
}

// ---- Commands from other tasks ----

static void on_cmd_event(struct ble_npl_event *ev)
{
    (void)ev;
    phone_cmd_t cmd;
    while (xQueueReceive(s_cmds, &cmd, 0) == pdTRUE) {
        if (cmd.kind == CMD_FORGET) {
            ESP_LOGI(TAG, "forgetting the paired phone");
            ble_store_clear();
            if (s_conn != BLE_HS_CONN_HANDLE_NONE) {
                ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
            }
            continue;
        }
        if (cmd.kind == CMD_ENABLE) {
            if (!s_enabled) {
                ESP_LOGI(TAG, "Bluetooth off");
                ble_gap_adv_stop();
                if (s_conn != BLE_HS_CONN_HANDLE_NONE) {
                    ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
                }
            } else if (s_conn == BLE_HS_CONN_HANDLE_NONE) {
                ESP_LOGI(TAG, "Bluetooth on");
                advertise(true);
            }
            continue;
        }
        if (!s_setup_done) {
            continue;
        }
        if (cmd.kind == CMD_LINK_SPEED) {
            apply_link_params();
        } else if (cmd.kind == CMD_MEDIA) {
            write_enqueue(s_remote_cmd, &cmd.arg, 1, false);
        } else {
            const uint8_t data[] = {
                CMD_PERFORM_ACTION,
                cmd.uid & 0xFF, (cmd.uid >> 8) & 0xFF, (cmd.uid >> 16) & 0xFF, cmd.uid >> 24,
                cmd.arg,
            };
            write_enqueue(s_ctrl_pt, data, sizeof(data), false);
        }
    }
}

static void send_cmd(const phone_cmd_t *cmd)
{
    if (!s_cmds || xQueueSend(s_cmds, cmd, 0) != pdTRUE) {
        return;
    }
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_cmd_event);
}

void phone_media_command(phone_media_cmd_t cmd)
{
    phone_cmd_t c = {.kind = CMD_MEDIA, .arg = (uint8_t)cmd};
    send_cmd(&c);
}

void phone_set_interactive(bool interactive)
{
    if (s_interactive == interactive) {
        return;
    }
    s_interactive = interactive;
    phone_cmd_t c = {.kind = CMD_LINK_SPEED};
    send_cmd(&c);
}

void phone_notification_action(uint32_t uid, bool positive)
{
    // ANCS action IDs: 0 positive, 1 negative.
    phone_cmd_t c = {.kind = CMD_NOTIF_ACTION, .arg = positive ? 0 : 1, .uid = uid};
    send_cmd(&c);
}

void phone_forget(void)
{
    phone_cmd_t c = {.kind = CMD_FORGET};
    send_cmd(&c);
}

void phone_set_enabled(bool enabled)
{
    if (s_enabled == enabled) {
        return;
    }
    s_enabled = enabled;
    phone_cmd_t c = {.kind = CMD_ENABLE};
    send_cmd(&c);
}

// ---- GAP ----

static int gap_event(struct ble_gap_event *ev, void *arg)
{
    (void)arg;
    struct ble_gap_conn_desc desc;

    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status != 0) {
            advertise(true);
            return 0;
        }
        s_conn = ev->connect.conn_handle;
        if (!s_enabled) {
            // Bluetooth was turned off while this connection was being made.
            ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }
        reset_link_state();
        post_simple(PHONE_EVT_CONNECTED);
        ble_gattc_exchange_mtu(s_conn, NULL, NULL);
        // Encrypts with stored keys, or starts pairing with a new phone.
        ble_gap_security_initiate(s_conn);
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected (reason 0x%x)", ev->disconnect.reason);
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        reset_link_state();
        post_simple(PHONE_EVT_DISCONNECTED);
        advertise(true);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (ev->enc_change.status == 0) {
            post_simple(PHONE_EVT_SECURED);
            run_step(STEP_ANCS_SVC);
        } else {
            ESP_LOGW(TAG, "encryption failed: %d", ev->enc_change.status);
            post_simple(PHONE_EVT_PAIRING_FAILED);
            // The phone may have forgotten us; drop our old keys so pairing can start fresh.
            if (ble_gap_conn_find(ev->enc_change.conn_handle, &desc) == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
            }
            ble_gap_terminate(ev->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        return 0;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if (ev->passkey.params.action == BLE_SM_IOACT_DISP) {
            struct ble_sm_io io = {.action = BLE_SM_IOACT_DISP, .passkey = esp_random() % 1000000};
            ble_sm_inject_io(ev->passkey.conn_handle, &io);
            phone_event_t evt = {.type = PHONE_EVT_PASSKEY, .passkey = io.passkey};
            post(&evt);
        }
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        if (ble_gap_conn_find(ev->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_NOTIFY_RX: {
        uint8_t buf[256];
        uint16_t len = 0;
        if (ble_hs_mbuf_to_flat(ev->notify_rx.om, buf, sizeof(buf), &len) != 0) {
            return 0;
        }
        uint16_t h = ev->notify_rx.attr_handle;
        if (h == s_notif_src) {
            on_notification_source(buf, len);
        } else if (h == s_data_src) {
            on_data_source(buf, len);
        } else if (h == s_entity) {
            on_entity_update(buf, len);
        } else if (h == s_time) {
            on_time_value(buf, len);
        }
        return 0;
    }

    case BLE_GAP_EVENT_CONN_UPDATE:
        if (ble_gap_conn_find(ev->conn_update.conn_handle, &desc) == 0) {
            ESP_LOGI(TAG, "connection interval %d ms, latency %d%s",
                     desc.conn_itvl * 5 / 4, desc.conn_latency,
                     ev->conn_update.status ? " (update refused)" : "");
        }
        if (s_params_pending) {
            s_params_pending = false;
            s_params_applied = ev->conn_update.status == 0 ? s_params_requested : -1;
        }
        // The screen may have changed state while we waited.
        apply_link_params();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (s_conn == BLE_HS_CONN_HANDLE_NONE) {
            advertise(false);
        }
        return 0;

    default:
        return 0;
    }
}

// Fast advertising for 30s so pairing and reconnecting are quick, then about once a second.
static void advertise(bool fast)
{
    if (!s_enabled) {
        return;
    }
    struct ble_hs_adv_fields fields = {
        .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
        // Asking for ANCS makes the watch show up in the iPhone's Bluetooth settings.
        .sol_uuids128 = &ANCS_SVC,
        .sol_num_uuids128 = 1,
    };
    ble_gap_adv_set_fields(&fields);

    const char *name = ble_svc_gap_device_name();
    struct ble_hs_adv_fields rsp = {
        .name = (const uint8_t *)name,
        .name_len = strlen(name),
        .name_is_complete = 1,
    };
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = fast ? BLE_GAP_ADV_ITVL_MS(20) : BLE_GAP_ADV_ITVL_MS(1000),
        .itvl_max = fast ? BLE_GAP_ADV_ITVL_MS(30) : BLE_GAP_ADV_ITVL_MS(1022),
    };
    int rc = ble_gap_adv_start(s_own_addr_type, NULL, fast ? ADV_FAST_MS : BLE_HS_FOREVER,
                               &params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "advertising failed: %d", rc);
    }
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &s_own_addr_type);
    advertise(true);
    if (s_enabled) {
        ESP_LOGI(TAG, "advertising as %s", ble_svc_gap_device_name());
    }
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "Bluetooth reset, reason %d", reason);
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t phone_init(QueueHandle_t events)
{
    s_events = events;
    s_cmds = xQueueCreate(CMD_QUEUE_LEN, sizeof(phone_cmd_t));

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth init failed: %s", esp_err_to_name(err));
        return err;
    }
    ble_npl_event_init(&s_cmd_event, on_cmd_event, NULL);

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // The watch shows a 6-digit code, the user types it on the phone.
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_store_config_init();

    nimble_port_freertos_init(host_task);
    return ESP_OK;
}
