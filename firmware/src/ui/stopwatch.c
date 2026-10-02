#include "stopwatch.h"

#include <stdio.h>

#include "esp_timer.h"
#include "ui/theme.h"

#define CONTENT_W   200
#define REFRESH_MS  100
#define LAPS_SHOWN  3

static bool s_running;
static int64_t s_started_us;     // when running: start time, adjusted for earlier runs
static int64_t s_elapsed_us;     // when stopped
static int64_t s_laps[LAPS_SHOWN];   // split times, newest first
static int s_lap_count;
static int64_t s_last_lap_us;

static lv_obj_t *s_time;
static lv_obj_t *s_laps_label;
static lv_obj_t *s_play_label;
static lv_obj_t *s_left_label;
static lv_timer_t *s_refresh;

static int64_t elapsed_us(void)
{
    return s_running ? esp_timer_get_time() - s_started_us : s_elapsed_us;
}

static void format(char *buf, size_t len, int64_t us)
{
    int64_t tenths = us / 100000;
    int h = (int)(tenths / 36000);
    int m = (int)(tenths / 600 % 60);
    int s = (int)(tenths / 10 % 60);
    int t = (int)(tenths % 10);
    if (h > 0) {
        snprintf(buf, len, "%d:%02d:%02d", h, m, s);
    } else {
        snprintf(buf, len, "%02d:%02d.%d", m, s, t);
    }
}

static void refresh(void)
{
    if (!s_time) {
        return;
    }
    char buf[16];
    format(buf, sizeof(buf), elapsed_us());
    lv_label_set_text(s_time, buf);
    lv_obj_set_style_text_color(s_time, s_running ? UI_COLOR_ACCENT : lv_color_white(), 0);
    lv_label_set_text(s_play_label, s_running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    // Lap while running, reset while stopped.
    lv_label_set_text(s_left_label, s_running ? LV_SYMBOL_LOOP : LV_SYMBOL_REFRESH);
}

static void refresh_laps(void)
{
    char text[LAPS_SHOWN * 24] = "";
    size_t used = 0;
    int shown = s_lap_count < LAPS_SHOWN ? s_lap_count : LAPS_SHOWN;
    for (int i = 0; i < shown; i++) {
        char t[16];
        format(t, sizeof(t), s_laps[i]);
        used += snprintf(text + used, sizeof(text) - used, "%sLap %d  %s", i ? "\n" : "", s_lap_count - i, t);
    }
    lv_label_set_text(s_laps_label, text);
}

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    refresh();
}

static void on_play(lv_event_t *e)
{
    (void)e;
    if (s_running) {
        s_elapsed_us = elapsed_us();
        s_running = false;
        lv_timer_delete(s_refresh);
        s_refresh = NULL;
    } else {
        s_started_us = esp_timer_get_time() - s_elapsed_us;
        s_running = true;
        s_refresh = lv_timer_create(refresh_cb, REFRESH_MS, NULL);
    }
    refresh();
}

static void on_left(lv_event_t *e)
{
    (void)e;
    if (s_running) {
        int64_t now = elapsed_us();
        for (int i = LAPS_SHOWN - 1; i > 0; i--) {
            s_laps[i] = s_laps[i - 1];
        }
        s_laps[0] = now - s_last_lap_us;
        s_last_lap_us = now;
        s_lap_count++;
    } else {
        s_elapsed_us = 0;
        s_last_lap_us = 0;
        s_lap_count = 0;
    }
    refresh_laps();
    refresh();
}

void stopwatch_create(lv_obj_t *parent)
{
    lv_obj_t *heading = ui_label(parent, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(heading, "Stopwatch");
    lv_obj_align(heading, LV_ALIGN_CENTER, 0, -100);

    s_time = ui_label(parent, &lv_font_montserrat_48, lv_color_white());
    lv_obj_align(s_time, LV_ALIGN_CENTER, 0, -58);

    lv_obj_t *controls = lv_obj_create(parent);
    lv_obj_remove_style_all(controls);
    lv_obj_set_size(controls, CONTENT_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(controls, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *left = ui_round_button(controls, LV_SYMBOL_REFRESH, 48, UI_COLOR_BUTTON);
    lv_obj_add_event_cb(left, on_left, LV_EVENT_CLICKED, NULL);
    s_left_label = lv_obj_get_child(left, 0);
    lv_obj_t *play = ui_round_button(controls, LV_SYMBOL_PLAY, 64, UI_COLOR_ACCENT);
    lv_obj_add_event_cb(play, on_play, LV_EVENT_CLICKED, NULL);
    s_play_label = lv_obj_get_child(play, 0);

    s_laps_label = ui_label(parent, &lv_font_montserrat_16, UI_COLOR_DIM);
    lv_obj_set_style_text_align(s_laps_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_laps_label, LV_ALIGN_TOP_MID, 0, 162);
    lv_label_set_text(s_laps_label, "");

    refresh();
}

void stopwatch_on_show(void)
{
    refresh();
}
