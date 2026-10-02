#include "countdown.h"

#include "esp_timer.h"
#include "ui/theme.h"

#define CONTENT_W       200
#define DEFAULT_S       (5 * 60)
#define MAX_S           (99 * 60 + 59)
#define STEP_S          60
#define REFRESH_MS      250

typedef enum { TIMER_IDLE, TIMER_RUNNING, TIMER_PAUSED } timer_state_t;

static timer_state_t s_state;
static int32_t s_set_s = DEFAULT_S;    // what the timer resets to
static int32_t s_left_s = DEFAULT_S;   // when idle or paused
static int64_t s_end_us;               // when running

static lv_obj_t *s_time;
static lv_obj_t *s_play_label;
static lv_timer_t *s_refresh;

static int32_t seconds_left(void)
{
    if (s_state != TIMER_RUNNING) {
        return s_left_s;
    }
    int64_t left_us = s_end_us - esp_timer_get_time();
    // Round up, so it shows 00:01 until the very end rather than 00:00 for a second.
    return left_us <= 0 ? 0 : (int32_t)((left_us + 999999) / 1000000);
}

static void refresh(void)
{
    if (!s_time) {
        return;
    }
    int32_t s = seconds_left();
    lv_label_set_text_fmt(s_time, "%02ld:%02ld", (long)(s / 60), (long)(s % 60));
    lv_obj_set_style_text_color(s_time, s_state == TIMER_IDLE ? lv_color_white() : UI_COLOR_ACCENT, 0);
    lv_label_set_text(s_play_label, s_state == TIMER_RUNNING ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    refresh();
}

static void set_running(bool running)
{
    if (running) {
        s_end_us = esp_timer_get_time() + (int64_t)s_left_s * 1000000;
        s_state = TIMER_RUNNING;
        if (!s_refresh) {
            s_refresh = lv_timer_create(refresh_cb, REFRESH_MS, NULL);
        }
    } else {
        s_left_s = seconds_left();
        s_state = TIMER_PAUSED;
        if (s_refresh) {
            lv_timer_delete(s_refresh);
            s_refresh = NULL;
        }
    }
}

static void reset(void)
{
    if (s_state == TIMER_RUNNING) {
        set_running(false);
    }
    s_state = TIMER_IDLE;
    s_left_s = s_set_s;
}

static void on_play(lv_event_t *e)
{
    (void)e;
    if (s_state == TIMER_RUNNING) {
        set_running(false);
    } else if (s_left_s > 0) {
        set_running(true);
    }
    refresh();
}

static void adjust(int32_t delta_s)
{
    if (s_state == TIMER_RUNNING) {
        int64_t min_end = esp_timer_get_time() + 1000000;
        s_end_us += (int64_t)delta_s * 1000000;
        s_end_us = s_end_us < min_end ? min_end : s_end_us;
        return;
    }
    int32_t s = s_left_s + delta_s;
    s = s < 0 ? 0 : (s > MAX_S ? MAX_S : s);
    s_left_s = s;
    if (s_state == TIMER_IDLE) {
        s_set_s = s;
    }
}

static void on_adjust(lv_event_t *e)
{
    adjust((int32_t)(intptr_t)lv_event_get_user_data(e));
    refresh();
}

static void on_preset(lv_event_t *e)
{
    reset();
    s_set_s = s_left_s = (int32_t)(intptr_t)lv_event_get_user_data(e);
    refresh();
}

static void on_reset(lv_event_t *e)
{
    (void)e;
    reset();
    refresh();
}

static lv_obj_t *row(lv_obj_t *parent, int32_t y)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, CONTENT_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(r, LV_ALIGN_CENTER, 0, y);
    return r;
}

static void add_adjust_button(lv_obj_t *parent, const char *text, int32_t delta_s)
{
    lv_obj_t *btn = ui_round_button(parent, text, 48, UI_COLOR_BUTTON);
    // Holding the button keeps adjusting.
    lv_obj_add_event_cb(btn, on_adjust, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)delta_s);
    lv_obj_add_event_cb(btn, on_adjust, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)delta_s);
}

static void add_preset_button(lv_obj_t *parent, const char *text, int32_t seconds)
{
    lv_obj_t *btn = ui_round_button(parent, text, 44, UI_COLOR_BUTTON);
    lv_obj_set_style_text_font(lv_obj_get_child(btn, 0), &lv_font_montserrat_16, 0);
    lv_obj_add_event_cb(btn, on_preset, LV_EVENT_CLICKED, (void *)(intptr_t)seconds);
}

void countdown_create(lv_obj_t *parent)
{
    lv_obj_t *heading = ui_label(parent, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(heading, "Timer");
    lv_obj_align(heading, LV_ALIGN_CENTER, 0, -96);

    s_time = ui_label(parent, &lv_font_montserrat_48, lv_color_white());
    lv_obj_align(s_time, LV_ALIGN_CENTER, 0, -50);

    lv_obj_t *controls = row(parent, 14);
    add_adjust_button(controls, LV_SYMBOL_MINUS, -STEP_S);
    lv_obj_t *play = ui_round_button(controls, LV_SYMBOL_PLAY, 64, UI_COLOR_ACCENT);
    lv_obj_add_event_cb(play, on_play, LV_EVENT_CLICKED, NULL);
    s_play_label = lv_obj_get_child(play, 0);
    add_adjust_button(controls, LV_SYMBOL_PLUS, STEP_S);

    lv_obj_t *presets = row(parent, 80);
    add_preset_button(presets, "1m", 60);
    add_preset_button(presets, "5m", 5 * 60);
    add_preset_button(presets, "10m", 10 * 60);
    lv_obj_t *rst = ui_round_button(presets, LV_SYMBOL_REFRESH, 44, UI_COLOR_BUTTON);
    lv_obj_add_event_cb(rst, on_reset, LV_EVENT_CLICKED, NULL);

    refresh();
}

TickType_t countdown_ticks_until_done(void)
{
    if (s_state != TIMER_RUNNING) {
        return portMAX_DELAY;
    }
    int64_t left_us = s_end_us - esp_timer_get_time();
    if (left_us <= 0) {
        return 0;
    }
    TickType_t ticks = pdMS_TO_TICKS((uint32_t)((left_us + 999) / 1000));
    return ticks > 0 ? ticks : 1;
}

bool countdown_check_done(void)
{
    if (s_state != TIMER_RUNNING || esp_timer_get_time() < s_end_us) {
        return false;
    }
    reset();
    refresh();
    return true;
}
