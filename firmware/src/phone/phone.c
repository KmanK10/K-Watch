// NimBLE peripheral that acts as a GATT client to the iPhone's ANCS and CTS.
//
// After the link is encrypted, setup runs as a chain of GATT operations:
// find services -> find characteristics -> find CCCDs -> subscribe -> read time.
// Each step's completion callback starts the next one.

#include "phone.h"

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

// 7905F431-B5CE-4E99-A40F-4B1E122D00D0 and friends, bytes reversed.
static const ble_uuid128_t ANCS_SVC = BLE_UUID128_INIT(
    0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79);
static const ble_uuid128_t ANCS_NOTIF_SRC = BLE_UUID128_INIT(
    0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C, 0xD9, 0x42, 0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F);
static const ble_uuid128_t ANCS_CTRL_PT = BLE_UUID128_INIT(
    0xD9, 0xD9, 0xAA, 0xFD, 0xBD, 0x9B, 0x21, 0x98, 0xA8, 0x49, 0xE1, 0x45, 0xF3, 0xD8, 0xD1, 0x69);
static const ble_uuid128_t ANCS_DATA_SRC = BLE_UUID128_INIT(
    0xFB, 0x7B, 0x7C, 0xCE, 0x6A, 0xB3, 0x44, 0xBE, 0xB5, 0x4B, 0xD6, 0x24, 0xE9, 0xC6, 0xEA, 0x22);
static const ble_uuid16_t CTS_SVC = BLE_UUID16_INIT(0x1805);
static const ble_uuid16_t CTS_CURRENT_TIME = BLE_UUID16_INIT(0x2A2B);
static const ble_uuid16_t CCCD = BLE_UUID16_INIT(BLE_GATT_DSC_CLT_CFG_UUID16);

// ANCS protocol values
#define EVT_ADDED            0
#define EVT_REMOVED          2
#define FLAG_PRE_EXISTING    (1 << 2)
#define CMD_GET_NOTIF_ATTRS  0
#define ATTR_APP_ID          0
#define ATTR_TITLE           1
#define ATTR_MESSAGE         3
#define ATTR_COUNT           3

#define ADV_FAST_MS          30000
#define MAX_CHRS             8
#define PENDING_MAX          8
#define DATA_BUF_SIZE        512

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
    STEP_DONE,
} setup_step_t;

typedef struct {
    uint16_t start;
    uint16_t end;
    uint16_t chr_vals[MAX_CHRS];   // every characteristic, to tell which one owns a CCCD
    int chr_count;
} service_t;

typedef struct {
    uint32_t uid;
    uint8_t category;
} pending_t;

static QueueHandle_t s_events;
static uint8_t s_own_addr_type;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;

static service_t s_ancs, s_cts;
static uint16_t s_notif_src, s_notif_src_cccd;
static uint16_t s_ctrl_pt;
static uint16_t s_data_src, s_data_src_cccd;
static uint16_t s_time, s_time_cccd;

static pending_t s_pending[PENDING_MAX];
static int s_pending_count;
static bool s_request_active;
static uint8_t s_data[DATA_BUF_SIZE];
static size_t s_data_len;

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

static void reset_link_state(void)
{
    memset(&s_ancs, 0, sizeof(s_ancs));
    memset(&s_cts, 0, sizeof(s_cts));
    s_notif_src = s_notif_src_cccd = s_ctrl_pt = 0;
    s_data_src = s_data_src_cccd = s_time = s_time_cccd = 0;
    s_pending_count = 0;
    s_request_active = false;
    s_data_len = 0;
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

static int on_ctrl_pt_written(uint16_t conn, const struct ble_gatt_error *err,
                              struct ble_gatt_attr *attr, void *arg)
{
    (void)conn;
    (void)attr;
    (void)arg;
    if (err->status != 0) {
        // Usually the notification was removed before we asked for it.
        ESP_LOGW(TAG, "ANCS request failed: %d", err->status);
        finish_request();
    }
    return 0;
}

static void request_next_notification(void)
{
    if (s_request_active || s_pending_count == 0 || !s_ctrl_pt) {
        return;
    }
    uint32_t uid = s_pending[0].uid;
    const uint8_t cmd[] = {
        CMD_GET_NOTIF_ATTRS,
        uid & 0xFF, (uid >> 8) & 0xFF, (uid >> 16) & 0xFF, uid >> 24,
        ATTR_APP_ID,
        ATTR_TITLE, sizeof(((phone_notification_t *)0)->title) - 1, 0,
        ATTR_MESSAGE, sizeof(((phone_notification_t *)0)->message) - 1, 0,
    };
    s_request_active = true;
    s_data_len = 0;
    if (ble_gattc_write_flat(s_conn, s_ctrl_pt, cmd, sizeof(cmd), on_ctrl_pt_written, NULL) != 0) {
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
        } else if (id == ATTR_MESSAGE) {
            copy_attr(n->message, sizeof(n->message), val, len);
        }
        pos += 3 + len;
    }
    n->uid = s_data[1] | (s_data[2] << 8) | (s_data[3] << 16) | ((uint32_t)s_data[4] << 24);
    return true;
}

static void on_data_source(const uint8_t *data, size_t len)
{
    if (!s_request_active) {
        return;
    }
    if (s_data_len + len > sizeof(s_data)) {
        ESP_LOGW(TAG, "notification too large, skipped");
        finish_request();
        return;
    }
    memcpy(&s_data[s_data_len], data, len);
    s_data_len += len;

    phone_event_t evt = {.type = PHONE_EVT_NOTIFICATION};
    if (parse_data_source(&evt.notification)) {
        evt.notification.category = s_pending[0].category;
        ESP_LOGI(TAG, "notification %lu from %s", (unsigned long)evt.notification.uid,
                 evt.notification.app_id);
        post(&evt);
        finish_request();
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

    if (event_id == EVT_ADDED && !(flags & FLAG_PRE_EXISTING)) {
        if (s_pending_count < PENDING_MAX) {
            s_pending[s_pending_count++] = (pending_t){.uid = uid, .category = category};
            request_next_notification();
        }
    } else if (event_id == EVT_REMOVED) {
        phone_event_t evt = {.type = PHONE_EVT_NOTIFICATION_REMOVED, .uid = uid};
        post(&evt);
    }
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
    run_step(STEP_DONE);
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
    run_step(s == &s_ancs ? STEP_ANCS_CHRS : STEP_CTS_CHRS);
    return 0;
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
        if (s == &s_ancs) {
            if (ble_uuid_cmp(&chr->uuid.u, &ANCS_NOTIF_SRC.u) == 0) {
                s_notif_src = chr->val_handle;
            } else if (ble_uuid_cmp(&chr->uuid.u, &ANCS_CTRL_PT.u) == 0) {
                s_ctrl_pt = chr->val_handle;
            } else if (ble_uuid_cmp(&chr->uuid.u, &ANCS_DATA_SRC.u) == 0) {
                s_data_src = chr->val_handle;
            }
        } else if (ble_uuid_cmp(&chr->uuid.u, &CTS_CURRENT_TIME.u) == 0) {
            s_time = chr->val_handle;
        }
        return 0;
    }
    run_step(s == &s_ancs ? STEP_ANCS_DSCS : STEP_CTS_DSCS);
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
            if (owner && owner == s_notif_src) {
                s_notif_src_cccd = dsc->handle;
            } else if (owner && owner == s_data_src) {
                s_data_src_cccd = dsc->handle;
            } else if (owner && owner == s_time) {
                s_time_cccd = dsc->handle;
            }
        }
        return 0;
    }
    run_step(s == &s_ancs ? STEP_SUB_DATA_SRC : STEP_SUB_TIME);
    return 0;
}

static int on_subscribed(uint16_t conn, const struct ble_gatt_error *err,
                         struct ble_gatt_attr *attr, void *arg)
{
    (void)conn;
    (void)attr;
    if (err->status != 0) {
        ESP_LOGW(TAG, "subscribe failed: %d", err->status);
    }
    run_step((setup_step_t)(uintptr_t)arg);
    return 0;
}

static bool subscribe(uint16_t cccd, setup_step_t next)
{
    static const uint8_t enable[2] = {0x01, 0x00};
    return cccd && ble_gattc_write_flat(s_conn, cccd, enable, sizeof(enable), on_subscribed,
                                        (void *)(uintptr_t)next) == 0;
}

static void request_power_saving_params(void)
{
    // Within Apple's accessory guidelines: max >= min + 15ms, max * (latency + 1) <= 2s.
    struct ble_gap_upd_params p = {
        .itvl_min = BLE_GAP_CONN_ITVL_MS(150),
        .itvl_max = BLE_GAP_CONN_ITVL_MS(180),
        .latency = 4,
        .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(6000),
    };
    ble_gap_update_params(s_conn, &p);
}

static void run_step(setup_step_t step)
{
    if (s_conn == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    int rc = 0;
    switch (step) {
    case STEP_ANCS_SVC:
        rc = ble_gattc_disc_svc_by_uuid(s_conn, &ANCS_SVC.u, on_svc, &s_ancs);
        break;
    case STEP_ANCS_CHRS:
        if (!s_ancs.start) {
            ESP_LOGW(TAG, "iPhone did not offer notifications (ANCS)");
            run_step(STEP_CTS_SVC);
            return;
        }
        rc = ble_gattc_disc_all_chrs(s_conn, s_ancs.start, s_ancs.end, on_chr, &s_ancs);
        break;
    case STEP_ANCS_DSCS:
        rc = ble_gattc_disc_all_dscs(s_conn, s_ancs.start, s_ancs.end, on_dsc, &s_ancs);
        break;
    case STEP_SUB_DATA_SRC:
        // Apple asks for Data Source to be subscribed before Notification Source.
        if (!subscribe(s_data_src_cccd, STEP_SUB_NOTIF_SRC)) {
            run_step(STEP_SUB_NOTIF_SRC);
        }
        return;
    case STEP_SUB_NOTIF_SRC:
        if (!subscribe(s_notif_src_cccd, STEP_CTS_SVC)) {
            run_step(STEP_CTS_SVC);
        }
        return;
    case STEP_CTS_SVC:
        rc = ble_gattc_disc_svc_by_uuid(s_conn, &CTS_SVC.u, on_svc, &s_cts);
        break;
    case STEP_CTS_CHRS:
        if (!s_cts.start) {
            run_step(STEP_DONE);
            return;
        }
        rc = ble_gattc_disc_all_chrs(s_conn, s_cts.start, s_cts.end, on_chr, &s_cts);
        break;
    case STEP_CTS_DSCS:
        rc = ble_gattc_disc_all_dscs(s_conn, s_cts.start, s_cts.end, on_dsc, &s_cts);
        break;
    case STEP_SUB_TIME:
        if (!subscribe(s_time_cccd, STEP_READ_TIME)) {
            run_step(STEP_READ_TIME);
        }
        return;
    case STEP_READ_TIME:
        if (!s_time) {
            run_step(STEP_DONE);
            return;
        }
        rc = ble_gattc_read(s_conn, s_time, on_time_read, NULL);
        break;
    case STEP_DONE:
        ESP_LOGI(TAG, "setup done: notifications %s, time %s",
                 s_notif_src_cccd ? "yes" : "no", s_time ? "yes" : "no");
        request_power_saving_params();
        return;
    }
    if (rc != 0) {
        ESP_LOGW(TAG, "setup step %d failed: %d", step, rc);
    }
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
        } else if (h == s_time) {
            on_time_value(buf, len);
        }
        return 0;
    }

    case BLE_GAP_EVENT_CONN_UPDATE:
        if (ble_gap_conn_find(ev->conn_update.conn_handle, &desc) == 0) {
            ESP_LOGI(TAG, "connection interval %d ms, latency %d",
                     desc.conn_itvl * 5 / 4, desc.conn_latency);
        }
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
    ESP_LOGI(TAG, "advertising as %s", ble_svc_gap_device_name());
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

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth init failed: %s", esp_err_to_name(err));
        return err;
    }

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
