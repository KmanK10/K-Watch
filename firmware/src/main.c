#include <stdbool.h>
#include <time.h>

#include "board.h"
#include "display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "pmu.h"
#include "ui/watchface.h"

#define SCREEN_TIMEOUT_MS  5000
#define TAP_TO_WAKE        true

static const char *TAG = "k-watch";

static QueueHandle_t s_events;
static bool s_screen_on;
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
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_ui();
}

static void screen_on(void)
{
    if (s_screen_on) {
        display_trigger_activity();
        return;
    }
    board_set_screen_awake(true);
    board_set_touch_wake(false);
    refresh_ui();
    display_wake();
    s_screen_on = true;
    s_screen_on_since_us = esp_timer_get_time();
    ESP_LOGI(TAG, "screen on");
}

static void screen_off(void)
{
    if (!s_screen_on) {
        return;
    }
    display_sleep();
    board_set_touch_wake(TAP_TO_WAKE);
    board_set_screen_awake(false);
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

void app_main(void)
{
    s_events = xQueueCreate(8, sizeof(board_event_t));
    ESP_ERROR_CHECK(board_init(s_events));

    watchface_create();
    lv_timer_create(refresh_timer_cb, 1000, NULL);
    screen_on();

    while (true) {
        TickType_t wait = portMAX_DELAY;

        if (s_screen_on) {
            uint32_t next_ms = display_run();
            uint32_t inactive = display_inactive_ms();
            if (inactive >= SCREEN_TIMEOUT_MS) {
                screen_off();
                continue;
            }
            uint32_t until_off = SCREEN_TIMEOUT_MS - inactive;
            uint32_t ms = next_ms < until_off ? next_ms : until_off;
            wait = pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
        }

        board_event_t evt;
        if (xQueueReceive(s_events, &evt, wait) != pdTRUE) {
            continue;
        }
        switch (evt) {
        case BOARD_EVT_PMU:
            handle_pmu();
            break;
        case BOARD_EVT_TOUCH:
            screen_on();
            break;
        }
    }
}
