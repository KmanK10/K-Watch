#include <stdbool.h>
#include <time.h>

#include "board.h"
#include "display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "haptics.h"
#include "imu.h"
#include "lvgl.h"
#include "phone/phone.h"
#include "pmu.h"
#include "ui/music.h"
#include "ui/notify.h"
#include "ui/pairing.h"
#include "ui/screens.h"
#include "ui/watchface.h"

#define SCREEN_TIMEOUT_MS  6000
#define NOTIFY_TIMEOUT_MS  10000
#define DIM_BEFORE_OFF_MS  2000
#define NO_TIMEOUT         UINT32_MAX
#define TAP_TO_WAKE        true
#define WRIST_WAKE         true

static const char *TAG = "k-watch";

static QueueHandle_t s_board_events;
static QueueHandle_t s_phone_events;
static QueueSetHandle_t s_event_set;

static bool s_screen_on;
static bool s_dimmed;
static uint32_t s_timeout_ms = SCREEN_TIMEOUT_MS;
static int64_t s_screen_on_since_us;

static void refresh_ui(void)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    watchface_set_time(&t);

    pmu_status_t p;
    pmu_get_status(&p);
    watchface_set_power(p.battery_percent, p.charging, p.usb_connected);
    watchface_set_steps(imu_get_steps());
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_ui();
}

// Turns the screen on (or keeps it on) and sets how long it stays on without a touch.
static void screen_on_for(uint32_t timeout_ms)
{
    s_timeout_ms = timeout_ms;
    if (s_screen_on) {
        display_trigger_activity();
        return;
    }
    board_set_screen_awake(true);
    board_set_touch_wake(false);
    phone_set_interactive(true);
    refresh_ui();
    display_wake();
    s_screen_on = true;
    s_dimmed = false;
    s_screen_on_since_us = esp_timer_get_time();
    ESP_LOGI(TAG, "screen on");
}

static void screen_on(void)
{
    screen_on_for(SCREEN_TIMEOUT_MS);
}

static void screen_off(void)
{
    if (!s_screen_on) {
        return;
    }
    display_sleep();
    // The next wake should show the time, not an old notification.
    ui_show_home(false);
    board_set_touch_wake(TAP_TO_WAKE);
    board_set_screen_awake(false);
    phone_set_interactive(false);
    s_screen_on = false;
    ESP_LOGI(TAG, "screen off after %lld ms", (esp_timer_get_time() - s_screen_on_since_us) / 1000);
}

static void handle_pmu(void)
{
    uint32_t evt = pmu_read_events();
    board_rearm(BOARD_EVT_PMU);

    if (evt & PMU_EVT_BUTTON_SHORT) {
        if (s_screen_on) {
            screen_off();
        } else {
            screen_on();
        }
    }
    if (evt & (PMU_EVT_USB_IN | PMU_EVT_CHARGE_START)) {
        screen_on();
    }
    if (evt & ~(uint32_t)PMU_EVT_BUTTON_SHORT) {
        ESP_LOGI(TAG, "pmu events 0x%02lx", (unsigned long)evt);
    }
}

static void handle_imu(void)
{
    uint32_t evt = imu_read_events();
    board_rearm(BOARD_EVT_IMU);

    if ((evt & IMU_EVT_WRIST_TILT) && WRIST_WAKE && !s_screen_on) {
        ESP_LOGI(TAG, "wrist tilt");
        screen_on();
    }
}

static void handle_board_event(board_event_t evt)
{
    switch (evt) {
    case BOARD_EVT_PMU:
        handle_pmu();
        break;
    case BOARD_EVT_TOUCH:
        screen_on();
        break;
    case BOARD_EVT_IMU:
        handle_imu();
        break;
    }
}

static void leave_pairing_screen(void)
{
    if (pairing_is_showing()) {
        ui_close_overlay();
        screen_on();
    }
}

static void handle_phone_event(const phone_event_t *evt)
{
    switch (evt->type) {
    case PHONE_EVT_CONNECTED:
        ESP_LOGI(TAG, "phone connected");
        watchface_set_connected(true);
        music_set_connected(true);
        // The phone re-sends everything in its notification centre after connecting.
        notify_clear();
        break;
    case PHONE_EVT_DISCONNECTED:
        ESP_LOGI(TAG, "phone disconnected");
        watchface_set_connected(false);
        music_set_connected(false);
        leave_pairing_screen();
        break;
    case PHONE_EVT_PASSKEY:
        ESP_LOGI(TAG, "pairing code shown");
        pairing_show(evt->passkey);
        screen_on_for(NO_TIMEOUT);
        haptics_play(HAPTIC_TAP);
        break;
    case PHONE_EVT_SECURED:
        ESP_LOGI(TAG, "phone link secured");
        leave_pairing_screen();
        break;
    case PHONE_EVT_PAIRING_FAILED:
        ESP_LOGW(TAG, "pairing failed");
        leave_pairing_screen();
        break;
    case PHONE_EVT_NOTIFICATION: {
        const phone_notification_t *n = &evt->notification;
        notify_add(n);
        if (n->pre_existing || pairing_is_showing()) {
            break;
        }
        notify_show_card(n->uid);
        screen_on_for(NOTIFY_TIMEOUT_MS);
        if (!n->silent) {
            haptics_play(n->category == PHONE_CAT_INCOMING_CALL ? HAPTIC_ALERT : HAPTIC_NOTIFY);
        }
        break;
    }
    case PHONE_EVT_NOTIFICATION_REMOVED:
        // Read or dismissed on the phone, so drop it here too.
        notify_remove(evt->uid);
        break;
    case PHONE_EVT_MEDIA:
        music_update(&evt->media);
        break;
    case PHONE_EVT_TIME: {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &evt->time);
        ESP_LOGI(TAG, "time from phone: %s", buf);
        board_set_time(&evt->time);
        break;
    }
    }
}

static void update_dimming(uint32_t inactive_ms)
{
    bool dim = s_timeout_ms != NO_TIMEOUT && inactive_ms + DIM_BEFORE_OFF_MS >= s_timeout_ms;
    if (dim != s_dimmed) {
        s_dimmed = dim;
        display_set_dimmed(dim);
    }
}

// Runs LVGL and handles the screen timeout. Returns how long the loop may sleep.
static TickType_t run_screen(void)
{
    if (!s_screen_on) {
        return portMAX_DELAY;
    }
    uint32_t next_ms = display_run();
    if (s_timeout_ms == NO_TIMEOUT) {
        return pdMS_TO_TICKS(next_ms) > 0 ? pdMS_TO_TICKS(next_ms) : 1;
    }

    uint32_t inactive = display_inactive_ms();
    if (inactive >= s_timeout_ms) {
        screen_off();
        return portMAX_DELAY;
    }
    update_dimming(inactive);
    uint32_t dim_at = s_timeout_ms - DIM_BEFORE_OFF_MS;
    uint32_t until_change = inactive < dim_at ? dim_at - inactive : s_timeout_ms - inactive;
    uint32_t ms = next_ms < until_change ? next_ms : until_change;
    return pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
}

void app_main(void)
{
    s_board_events = xQueueCreate(8, sizeof(board_event_t));
    s_phone_events = xQueueCreate(8, sizeof(phone_event_t));
    s_event_set = xQueueCreateSet(8 + 8);
    xQueueAddToSet(s_board_events, s_event_set);
    xQueueAddToSet(s_phone_events, s_event_set);

    ESP_ERROR_CHECK(board_init(s_board_events));

    ui_init();
    lv_timer_create(refresh_timer_cb, 1000, NULL);
    screen_on();

    if (phone_init(s_phone_events) != ESP_OK) {
        ESP_LOGE(TAG, "running without Bluetooth");
    }

    while (true) {
        TickType_t wait = run_screen();
        QueueSetMemberHandle_t ready = xQueueSelectFromSet(s_event_set, wait);

        if (ready == s_board_events) {
            board_event_t evt;
            if (xQueueReceive(s_board_events, &evt, 0) == pdTRUE) {
                handle_board_event(evt);
            }
        } else if (ready == s_phone_events) {
            static phone_event_t evt;
            if (xQueueReceive(s_phone_events, &evt, 0) == pdTRUE) {
                handle_phone_event(&evt);
            }
        }
    }
}
