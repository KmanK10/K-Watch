#include "companion.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nimble/nimble_port.h"

#define MAX_MESSAGE      8192
#define FLAG_START       0x01
#define FLAG_END         0x02
#define OUT_QUEUE_LEN    6
// Out of Bluetooth buffers: wait this long, then carry on sending.
#define RETRY_MS         20

static const char *TAG = "companion";

// 58250B93-E11B-42C4-8104-7DDC9BD61EB2, and the next two numbers for RX and TX.
// NimBLE wants the bytes in reverse order.
const ble_uuid128_t COMPANION_SVC_UUID = BLE_UUID128_INIT(
    0xB2, 0x1E, 0xD6, 0x9B, 0xDC, 0x7D, 0x04, 0x81, 0xC4, 0x42, 0x1B, 0xE1, 0x93, 0x0B, 0x25, 0x58);
static const ble_uuid128_t RX_UUID = BLE_UUID128_INIT(
    0xB2, 0x1E, 0xD6, 0x9B, 0xDC, 0x7D, 0x04, 0x81, 0xC4, 0x42, 0x1B, 0xE1, 0x94, 0x0B, 0x25, 0x58);
static const ble_uuid128_t TX_UUID = BLE_UUID128_INIT(
    0xB2, 0x1E, 0xD6, 0x9B, 0xDC, 0x7D, 0x04, 0x81, 0xC4, 0x42, 0x1B, 0xE1, 0x95, 0x0B, 0x25, 0x58);

static uint16_t s_tx_handle;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static volatile bool s_subscribed;
static void (*s_on_message)(char *json);

// Incoming message being put together (Bluetooth task only).
static char *s_in;
static size_t s_in_len;
static bool s_in_started;

// Outgoing messages, sent one chunk at a time from the Bluetooth task.
static QueueHandle_t s_out_queue;
static char *s_out;
static size_t s_out_len;
static size_t s_out_sent;
static struct ble_npl_event s_send_event;
static struct ble_npl_callout s_retry;

static void on_chunk(const uint8_t *data, size_t len)
{
    if (len < 1) {
        return;
    }
    uint8_t flags = data[0];
    data++;
    len--;

    if (flags & FLAG_START) {
        s_in_len = 0;
        s_in_started = true;
    }
    if (!s_in_started) {
        return;
    }
    if (!s_in) {
        s_in = malloc(MAX_MESSAGE + 1);
        if (!s_in) {
            s_in_started = false;
            return;
        }
    }
    if (s_in_len + len > MAX_MESSAGE) {
        ESP_LOGW(TAG, "message too long, dropped");
        s_in_started = false;
        return;
    }
    memcpy(s_in + s_in_len, data, len);
    s_in_len += len;

    if (flags & FLAG_END) {
        s_in_started = false;
        char *msg = malloc(s_in_len + 1);
        if (!msg) {
            return;
        }
        memcpy(msg, s_in, s_in_len);
        msg[s_in_len] = '\0';
        if (s_on_message) {
            s_on_message(msg);
        } else {
            free(msg);
        }
    }
}

static int access_cb(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)attr;
    (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t buf[512];
    uint16_t len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &len) != 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    s_conn = conn;
    on_chunk(buf, len);
    return 0;
}

static const struct ble_gatt_svc_def s_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &COMPANION_SVC_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &RX_UUID.u,
                .access_cb = access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
            },
            {
                .uuid = &TX_UUID.u,
                .access_cb = access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_tx_handle,
            },
            {0},
        },
    },
    {0},
};

static void drop_outgoing(void)
{
    free(s_out);
    s_out = NULL;
    char *msg;
    while (s_out_queue && xQueueReceive(s_out_queue, &msg, 0) == pdTRUE) {
        free(msg);
    }
}

static void pump(void)
{
    while (s_subscribed) {
        if (!s_out) {
            if (xQueueReceive(s_out_queue, &s_out, 0) != pdTRUE) {
                return;
            }
            s_out_len = strlen(s_out);
            s_out_sent = 0;
        }
        // One byte of each chunk is the flags; the ATT header takes 3 more.
        size_t room = ble_att_mtu(s_conn) - 3 - 1;
        size_t n = s_out_len - s_out_sent < room ? s_out_len - s_out_sent : room;
        uint8_t chunk[256];
        n = n < sizeof(chunk) - 1 ? n : sizeof(chunk) - 1;
        chunk[0] = (s_out_sent == 0 ? FLAG_START : 0) | (s_out_sent + n == s_out_len ? FLAG_END : 0);
        memcpy(chunk + 1, s_out + s_out_sent, n);

        struct os_mbuf *om = ble_hs_mbuf_from_flat(chunk, n + 1);
        int rc = om ? ble_gatts_notify_custom(s_conn, s_tx_handle, om) : BLE_HS_ENOMEM;
        if (rc == BLE_HS_ENOMEM || rc == BLE_HS_EBUSY) {
            ble_npl_callout_reset(&s_retry, ble_npl_time_ms_to_ticks32(RETRY_MS));
            return;
        }
        if (rc != 0) {
            ESP_LOGW(TAG, "send failed: %d", rc);
            free(s_out);
            s_out = NULL;
            continue;
        }
        s_out_sent += n;
        if (s_out_sent == s_out_len) {
            free(s_out);
            s_out = NULL;
        }
    }
}

static void on_send_event(struct ble_npl_event *ev)
{
    (void)ev;
    pump();
}

void companion_register(void (*on_message)(char *json))
{
    s_on_message = on_message;
    s_out_queue = xQueueCreate(OUT_QUEUE_LEN, sizeof(char *));
    ble_npl_event_init(&s_send_event, on_send_event, NULL);
    ble_npl_callout_init(&s_retry, nimble_port_get_dflt_eventq(), on_send_event, NULL);
    int rc = ble_gatts_count_cfg(s_services);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(s_services);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "could not add the companion service: %d", rc);
    }
}

void companion_gap_event(const struct ble_gap_event *ev)
{
    switch (ev->type) {
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (ev->subscribe.attr_handle == s_tx_handle) {
            s_conn = ev->subscribe.conn_handle;
            s_subscribed = ev->subscribe.cur_notify;
            ESP_LOGI(TAG, "app %s", s_subscribed ? "connected" : "disconnected");
            if (s_subscribed) {
                pump();
            } else {
                drop_outgoing();
            }
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_subscribed = false;
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        s_in_started = false;
        drop_outgoing();
        break;
    default:
        break;
    }
}

bool companion_send(const char *json)
{
    if (!s_subscribed || !s_out_queue) {
        return false;
    }
    char *copy = strdup(json);
    if (!copy) {
        return false;
    }
    if (xQueueSend(s_out_queue, &copy, 0) != pdTRUE) {
        free(copy);
        return false;
    }
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_send_event);
    return true;
}

bool companion_ready(void)
{
    return s_subscribed;
}
