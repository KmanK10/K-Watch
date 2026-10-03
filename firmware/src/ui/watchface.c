#include "watchface.h"

#include <stdio.h>

#include "lvgl.h"
#include "ui/theme.h"
#include "ui/weather_icon.h"

#define GOAL_BAR_W     110
#define COLOR_GOAL_MET lv_color_hex(0x2EBD59)

static lv_obj_t *s_bluetooth;
static lv_obj_t *s_time;
static lv_obj_t *s_date;
static lv_obj_t *s_power;
static lv_obj_t *s_steps;
static lv_obj_t *s_goal_bar;
static lv_obj_t *s_dnd;
static lv_obj_t *s_alarm;
static lv_obj_t *s_weather;
static lv_obj_t *s_weather_icon;
static lv_obj_t *s_weather_temp;

static int s_last_minute = -1;
static int s_last_day = -1;
static int s_last_percent = -2;
static int s_last_flags = -1;
static uint32_t s_last_steps = UINT32_MAX;
static uint32_t s_last_goal;
static bool s_24h;

void watchface_create(lv_obj_t *parent)
{
    s_time = ui_label(parent, &lv_font_montserrat_48, lv_color_white());
    lv_obj_align(s_time, LV_ALIGN_CENTER, 0, 0);

    s_date = ui_label(parent, &lv_font_montserrat_20, UI_COLOR_ACCENT);
    lv_obj_align(s_date, LV_ALIGN_CENTER, 0, 44);

    s_power = ui_label(parent, &lv_font_montserrat_16, lv_color_white());
    lv_obj_align(s_power, LV_ALIGN_TOP_RIGHT, -8, 6);

    s_steps = ui_label(parent, &lv_font_montserrat_16, UI_COLOR_DIM);
    lv_obj_align(s_steps, LV_ALIGN_CENTER, 0, 76);

    s_goal_bar = lv_bar_create(parent);
    lv_obj_set_size(s_goal_bar, GOAL_BAR_W, 6);
    lv_obj_align(s_goal_bar, LV_ALIGN_CENTER, 0, 97);
    lv_obj_set_style_bg_color(s_goal_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(s_goal_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_goal_bar, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_bar_set_range(s_goal_bar, 0, GOAL_BAR_W);

    s_bluetooth = ui_label(parent, &lv_font_montserrat_16, lv_palette_main(LV_PALETTE_BLUE));
    lv_label_set_text(s_bluetooth, LV_SYMBOL_BLUETOOTH);
    lv_obj_align(s_bluetooth, LV_ALIGN_TOP_LEFT, 10, 6);
    lv_obj_set_hidden(s_bluetooth, true);

    s_alarm = ui_label(parent, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_label_set_text(s_alarm, LV_SYMBOL_BELL);
    lv_obj_align(s_alarm, LV_ALIGN_TOP_LEFT, 32, 8);
    lv_obj_set_hidden(s_alarm, true);

    s_dnd = ui_label(parent, &lv_font_montserrat_14, lv_color_hex(0x9C8CFF));
    lv_label_set_text(s_dnd, "DND");
    lv_obj_align(s_dnd, LV_ALIGN_TOP_MID, 0, 7);
    lv_obj_set_hidden(s_dnd, true);

    s_weather = lv_obj_create(parent);
    lv_obj_remove_style_all(s_weather);
    lv_obj_set_size(s_weather, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_weather, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_weather, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_weather, 6, 0);
    lv_obj_set_scrollable(s_weather, false);
    lv_obj_set_clickable(s_weather, false);
    lv_obj_align(s_weather, LV_ALIGN_CENTER, 0, -60);
    s_weather_icon = weather_icon_create(s_weather, 28);
    s_weather_temp = weather_temp_create(s_weather, &lv_font_montserrat_20, lv_color_white());
    lv_obj_set_hidden(s_weather, true);
}

void watchface_set_weather(const weather_t *w)
{
    lv_obj_set_hidden(s_weather, w == NULL);
    if (!w) {
        return;
    }
    weather_icon_set(s_weather_icon, w->code, w->day);
    weather_temp_set(s_weather_temp, w->temp);
}

void watchface_set_dnd(bool on)
{
    lv_obj_set_hidden(s_dnd, !on);
}

void watchface_set_alarm(bool on)
{
    if (lv_obj_is_hidden(s_alarm) == on) {
        lv_obj_set_hidden(s_alarm, !on);
    }
}

void watchface_set_connected(bool connected)
{
    lv_obj_set_hidden(s_bluetooth, !connected);
}

void watchface_set_24h(bool on)
{
    if (on != s_24h) {
        s_24h = on;
        s_last_minute = -1;   // redraw on the next watchface_set_time
    }
}

void watchface_set_steps(uint32_t steps, uint32_t goal)
{
    if (steps == s_last_steps && goal == s_last_goal) {
        return;
    }
    s_last_steps = steps;
    s_last_goal = goal;
    lv_label_set_text_fmt(s_steps, "%lu steps", (unsigned long)steps);

    bool met = goal == 0 || steps >= goal;
    int32_t filled = met ? GOAL_BAR_W : (int32_t)((uint64_t)steps * GOAL_BAR_W / goal);
    lv_bar_set_value(s_goal_bar, filled, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_goal_bar, met ? COLOR_GOAL_MET : UI_COLOR_ACCENT, LV_PART_INDICATOR);
}

void watchface_set_time(const struct tm *t)
{
    char buf[24];

    if (t->tm_min != s_last_minute) {
        s_last_minute = t->tm_min;
        if (s_24h) {
            snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
        } else {
            int hour12 = t->tm_hour % 12 == 0 ? 12 : t->tm_hour % 12;
            snprintf(buf, sizeof(buf), "%d:%02d", hour12, t->tm_min);
        }
        lv_label_set_text(s_time, buf);
    }

    if (t->tm_mday != s_last_day) {
        s_last_day = t->tm_mday;
        strftime(buf, sizeof(buf), "%a %b %e", t);
        lv_label_set_text(s_date, buf);
    }
}

void watchface_set_power(int battery_percent, bool charging, bool usb_connected)
{
    int flags = (charging ? 1 : 0) | (usb_connected ? 2 : 0);
    if (battery_percent == s_last_percent && flags == s_last_flags) {
        return;
    }
    s_last_percent = battery_percent;
    s_last_flags = flags;

    if (battery_percent < 0) {
        lv_label_set_text(s_power, usb_connected ? LV_SYMBOL_USB " USB" : "--");
    } else if (charging) {
        lv_label_set_text_fmt(s_power, LV_SYMBOL_CHARGE " %d%%", battery_percent);
    } else {
        lv_label_set_text_fmt(s_power, "%d%%", battery_percent);
    }

    lv_color_t color = battery_percent >= 0 && battery_percent <= 20 && !charging
                           ? lv_palette_main(LV_PALETTE_RED)
                           : lv_color_white();
    lv_obj_set_style_text_color(s_power, color, 0);
}
