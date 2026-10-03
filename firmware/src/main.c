#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "alarms.h"
#include "board.h"
#include "display.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "haptics.h"
#include "imu.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "phone/phone.h"
#include "pmu.h"
#include "esp_app_desc.h"
#include "settings.h"
#include "steps.h"
#include "ui/alert.h"
#include "ui/countdown.h"
#include "ui/flashlight.h"
#include "ui/health_app.h"
#include "ui/music.h"
#include "ui/notify.h"
#include "ui/pairing.h"
#include "ui/screens.h"
#include "ui/settings_screen.h"
#include "ui/watchface.h"

#define NOTIFY_TIMEOUT_MS  10000
#define ALERT_TIMEOUT_MS   60000
#define FLASHLIGHT_TIMEOUT_MS (5 * 60 * 1000)
#define DIM_BEFORE_OFF_MS  2000
#define NO_TIMEOUT         UINT32_MAX
// Steps add up silently while the screen is off, so look at the goal this often until it's met.
#define GOAL_CHECK_TICKS   pdMS_TO_TICKS(60 * 1000)

static const char *TAG = "k-watch";

static QueueHandle_t s_board_events;
static QueueHandle_t s_phone_events;
static QueueSetHandle_t s_event_set;

static bool s_screen_on;
static bool s_dimmed;
static bool s_alarm_ringing;
static bool s_goal_reached;
static uint32_t s_timeout_ms;
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
    watchface_set_steps(steps_today());
    watchface_set_alarm(alarms_any_enabled());
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
    screen_on_for(settings_get()->screen_timeout_s * 1000);
}

static void screen_off(void)
{
    if (!s_screen_on) {
        return;
    }
    display_sleep();
    // An alarm nobody answered (or silenced with the button) rings again in a few minutes.
    if (s_alarm_ringing && alert_is_showing()) {
        alarms_snooze(true);
    }
    s_alarm_ringing = false;
    // The next wake should show the time, not an old notification or alert.
    alert_dismiss();
    ui_show_home(false);
    board_set_touch_wake(settings_get()->tap_to_wake);
    board_set_screen_awake(false);
    phone_set_interactive(false);
    steps_save(false);
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

    if ((evt & IMU_EVT_WRIST_TILT) && settings_get()->raise_to_wake && !s_screen_on) {
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
        haptics_play(HAPTIC_NOTIFY);
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
        // Silent means the phone didn't alert either (Focus, Do Not Disturb, or the app's
        // sounds are off), so it waits quietly in the list. The watch's own DND does the same.
        if (n->pre_existing || n->silent || settings_get()->dnd || pairing_is_showing() ||
            alert_is_showing() || flashlight_is_on()) {
            break;
        }
        notify_show_card(n->uid);
        screen_on_for(NOTIFY_TIMEOUT_MS);
        // A call that just started is already in hand on the phone; no need to buzz about it.
        if (settings_get()->notify_vibrate && n->category != PHONE_CAT_ACTIVE_CALL) {
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
        alarms_time_changed();
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

// Wake options take effect the next time the screen turns off.
static void apply_settings(const settings_t *s)
{
    display_set_brightness(s->brightness);
    watchface_set_24h(s->clock_24h);
    watchface_set_dnd(s->dnd);
    phone_set_enabled(s->bluetooth);
    haptics_set_touch_feedback(s->touch_feedback);
    if (s_screen_on && s_timeout_ms != NO_TIMEOUT) {
        s_timeout_ms = s->screen_timeout_s * 1000;
    }
}

static void stop_alarm(void)
{
    s_alarm_ringing = false;
    alarms_stop();
    screen_on();
}

static void snooze_alarm(void)
{
    s_alarm_ringing = false;
    alarms_snooze(false);
    screen_on();
}

static void ring_alarm(size_t which)
{
    char time[16];
    alarms_format_time(alarms_get(which), time, sizeof(time));
    ESP_LOGI(TAG, "alarm %s", time);
    alert_show("Alarm", time, stop_alarm, snooze_alarm);
    s_alarm_ringing = true;
    screen_on_for(ALERT_TIMEOUT_MS);
}

// Why the watch last restarted, if it wasn't a normal power-on or flash.
static const char *abnormal_restart_reason(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "froze";
    case ESP_RST_BROWNOUT: return "battery voltage dipped";
    default: return NULL;
    }
}

// percent is 0 when the flashlight turns off.
static void on_flashlight(uint8_t percent)
{
    display_set_brightness(percent ? percent : settings_get()->brightness);
    if (s_screen_on) {
        s_timeout_ms = percent ? FLASHLIGHT_TIMEOUT_MS : settings_get()->screen_timeout_s * 1000;
    }
}

// Celebrates once when today's steps pass the goal. Dropping back under it (a new day, or a
// higher goal) arms it again.
static void check_step_goal(void)
{
    bool reached = steps_today() >= steps_goal();
    if (!reached || s_goal_reached) {
        s_goal_reached = reached;
        return;
    }
    s_goal_reached = true;
    ESP_LOGI(TAG, "step goal reached");
    // Lowering the goal in the Health app shouldn't throw a party over the top of it.
    if (settings_get()->dnd || health_app_is_showing() || alert_is_showing() || pairing_is_showing() ||
        flashlight_is_on()) {
        return;
    }
    health_show_goal_reached();
    screen_on_for(NOTIFY_TIMEOUT_MS);
    haptics_play(HAPTIC_NOTIFY);
}

static void erase_and_restart(lv_timer_t *t)
{
    (void)t;
    ESP_LOGW(TAG, "factory reset");
    nvs_flash_erase();
    esp_restart();
}

// Wipes settings, alarms, steps, the app order and the iPhone pairing, then starts fresh.
static void factory_reset(void)
{
    phone_forget();
    // Long enough for "Erasing..." to be drawn and the phone link to close.
    lv_timer_t *t = lv_timer_create(erase_and_restart, 500, NULL);
    lv_timer_set_repeat_count(t, 1);
}

void app_main(void)
{
    s_board_events = xQueueCreate(8, sizeof(board_event_t));
    s_phone_events = xQueueCreate(8, sizeof(phone_event_t));
    s_event_set = xQueueCreateSet(8 + 8);
    xQueueAddToSet(s_board_events, s_event_set);
    xQueueAddToSet(s_phone_events, s_event_set);

    ESP_ERROR_CHECK(board_init(s_board_events));
    steps_init();
    // Already past it from before a restart; no second celebration.
    s_goal_reached = steps_today() >= steps_goal();
    settings_init();
    alarms_init();

    ui_init();
    apply_settings(settings_get());
    settings_on_change(apply_settings);
    settings_screen_on_forget(phone_forget);
    settings_screen_on_factory_reset(factory_reset);
    flashlight_on_change(on_flashlight);
    char about[96];
    const char *restart = abnormal_restart_reason();
    if (restart) {
        ESP_LOGW(TAG, "restarted after: %s", restart);
        snprintf(about, sizeof(about), "K-Watch %s\nLast restart: %s", esp_app_get_description()->version,
                 restart);
    } else {
        snprintf(about, sizeof(about), "K-Watch %s", esp_app_get_description()->version);
    }
    settings_screen_set_about(about);
    lv_timer_create(refresh_timer_cb, 1000, NULL);
    screen_on();

    if (phone_init(s_phone_events) != ESP_OK) {
        ESP_LOGE(TAG, "running without Bluetooth");
    }

    while (true) {
        TickType_t wait = run_screen();
        TickType_t until_midnight = steps_ticks_until_midnight();
        wait = until_midnight < wait ? until_midnight : wait;
        TickType_t until_timer = countdown_ticks_until_done();
        wait = until_timer < wait ? until_timer : wait;
        TickType_t until_alarm = alarms_ticks_until_next();
        wait = until_alarm < wait ? until_alarm : wait;
        if (!s_goal_reached && GOAL_CHECK_TICKS < wait) {
            wait = GOAL_CHECK_TICKS;
        }
        QueueSetMemberHandle_t ready = xQueueSelectFromSet(s_event_set, wait);
        steps_check_day();
        check_step_goal();
        if (countdown_check_done()) {
            ESP_LOGI(TAG, "timer done");
            s_alarm_ringing = false;
            alert_show("Time's up", "Timer finished", screen_on, NULL);
            screen_on_for(ALERT_TIMEOUT_MS);
        }
        size_t alarm;
        if (alarms_check_due(&alarm)) {
            ring_alarm(alarm);
        }

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
