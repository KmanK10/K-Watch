#include "watchface.h"

#include <stdio.h>

#include "lvgl.h"

#define COLOR_ACCENT lv_color_hex(0xFF5F1F)

static lv_obj_t *s_time;
static lv_obj_t *s_date;
static lv_obj_t *s_power;

static int s_last_minute = -1;
static int s_last_day = -1;
static int s_last_percent = -2;
static int s_last_flags = -1;

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, "");
    return label;
}

void watchface_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);

    s_time = make_label(scr, &lv_font_montserrat_48, lv_color_white());
    lv_obj_align(s_time, LV_ALIGN_CENTER, 0, 0);

    s_date = make_label(scr, &lv_font_montserrat_20, COLOR_ACCENT);
    lv_obj_align(s_date, LV_ALIGN_CENTER, 0, 44);

    s_power = make_label(scr, &lv_font_montserrat_16, lv_color_white());
    lv_obj_align(s_power, LV_ALIGN_TOP_RIGHT, -8, 6);
}

void watchface_set_time(const struct tm *t)
{
    char buf[24];

    if (t->tm_min != s_last_minute) {
        s_last_minute = t->tm_min;
        int hour12 = t->tm_hour % 12 == 0 ? 12 : t->tm_hour % 12;
        snprintf(buf, sizeof(buf), "%d:%02d", hour12, t->tm_min);
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
