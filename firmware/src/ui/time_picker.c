#include "time_picker.h"

#include <stdio.h>

#include "settings.h"
#include "ui/screens.h"
#include "ui/theme.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_hour;
static lv_obj_t *s_minute;
static lv_obj_t *s_ampm;      // NULL with the 24-hour clock
static void (*s_done)(uint16_t minutes);

static void on_save(lv_event_t *e)
{
    (void)e;
    uint32_t hour = lv_roller_get_selected(s_hour);
    if (s_ampm) {
        hour += lv_roller_get_selected(s_ampm) * 12;
    }
    uint16_t minutes = (uint16_t)(hour * 60 + lv_roller_get_selected(s_minute));
    ui_close_overlay();
    if (s_done) {
        s_done(minutes);
    }
}

static lv_obj_t *add_roller(lv_obj_t *parent, const char *options, lv_roller_mode_t mode, uint32_t selected)
{
    lv_obj_t *r = lv_roller_create(parent);
    lv_roller_set_options(r, options, mode);
    lv_roller_set_visible_row_count(r, 3);
    lv_roller_set_selected(r, selected, LV_ANIM_OFF);
    lv_obj_set_width(r, 62);
    lv_obj_set_style_text_font(r, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(r, UI_COLOR_DIM, 0);
    lv_obj_set_style_bg_color(r, lv_color_black(), 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_text_color(r, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(r, UI_COLOR_BUTTON, LV_PART_SELECTED);
    lv_obj_set_style_radius(r, 10, 0);
    // A sideways swipe on a wheel should still go back.
    lv_obj_set_gesture_bubble(r, true);
    return r;
}

void time_picker_open(const char *title, uint16_t minutes, void (*done)(uint16_t minutes))
{
    if (!s_screen) {
        s_screen = ui_screen_create();
        ui_on_swipe(s_screen, LV_EVENT_GESTURE_RIGHT, ui_close_overlay);
    }
    lv_obj_clean(s_screen);
    s_done = done;
    bool h24 = settings_get()->clock_24h;
    uint32_t hour = minutes / 60;

    lv_obj_t *heading = ui_label(s_screen, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(heading, title);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *wheels = lv_obj_create(s_screen);
    lv_obj_remove_style_all(wheels);
    lv_obj_set_size(wheels, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wheels, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wheels, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(wheels, 6, 0);
    lv_obj_set_scrollable(wheels, false);
    lv_obj_set_clickable(wheels, false);
    lv_obj_align(wheels, LV_ALIGN_TOP_MID, 0, 44);

    static char hours[24 * 3];
    static char mins[60 * 3];
    char *p = hours;
    for (int h = 0; h < (h24 ? 24 : 12); h++) {
        int shown = h24 ? h : (h == 0 ? 12 : h);
        p += sprintf(p, h24 ? "%s%02d" : "%s%d", h ? "\n" : "", shown);
    }
    p = mins;
    for (int m = 0; m < 60; m++) {
        p += sprintf(p, "%s%02d", m ? "\n" : "", m);
    }

    s_hour = add_roller(wheels, hours, LV_ROLLER_MODE_INFINITE, h24 ? hour : hour % 12);
    lv_obj_t *colon = ui_label(wheels, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(colon, ":");
    s_minute = add_roller(wheels, mins, LV_ROLLER_MODE_INFINITE, minutes % 60);
    s_ampm = h24 ? NULL : add_roller(wheels, "AM\nPM", LV_ROLLER_MODE_NORMAL, hour >= 12);
    // After layout, so placing the wheels doesn't count as turning them.
    lv_obj_update_layout(s_screen);
    ui_roller_haptics(s_hour);
    ui_roller_haptics(s_minute);
    if (s_ampm) {
        ui_roller_haptics(s_ampm);
    }

    lv_obj_t *save = ui_round_button(s_screen, "Save", 48, UI_COLOR_ACCENT);
    lv_obj_set_width(save, 140);
    lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_gesture_bubble(save, true);
    lv_obj_add_event_cb(save, on_save, LV_EVENT_CLICKED, NULL);

    ui_show_overlay(s_screen);
}
