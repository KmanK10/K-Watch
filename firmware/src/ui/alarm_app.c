#include "alarm_app.h"

#include <stdint.h>
#include <stdio.h>

#include "alarms.h"
#include "settings.h"
#include "ui/screens.h"
#include "ui/theme.h"

#define CONTENT_W      200
#define ROW_H          56
#define DAY_SIZE       26
#define NEW_ALARM      SIZE_MAX

static lv_obj_t *s_list;
static lv_obj_t *s_edit;

// Editor state
static size_t s_editing;
static lv_obj_t *s_hour;
static lv_obj_t *s_minute;
static lv_obj_t *s_ampm;      // NULL with the 24-hour clock
static lv_obj_t *s_days[7];
static uint8_t s_edit_days;

static void open_editor(size_t index);
static void rebuild_list(void);

static void rebuild_list_async(void *arg)
{
    (void)arg;
    rebuild_list();
}

// ---- List ----

static void on_row(lv_event_t *e)
{
    open_editor((size_t)(uintptr_t)lv_event_get_user_data(e));
}

static void on_switch(lv_event_t *e)
{
    size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    alarm_t a = *alarms_get(i);
    a.enabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    alarms_set(i, &a);
    // Rebuilding now would delete the switch in the middle of its own event.
    lv_async_call(rebuild_list_async, NULL);
}

static void on_add(lv_event_t *e)
{
    (void)e;
    open_editor(NEW_ALARM);
}

static lv_obj_t *add_row(int32_t height)
{
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, CONTENT_W, height);
    lv_obj_set_style_bg_color(row, UI_COLOR_BUTTON, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x484848), LV_STATE_PRESSED);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_clickable(row, true);
    lv_obj_set_gesture_bubble(row, true);
    lv_obj_set_scrollable(row, false);
    return row;
}

static void add_alarm_row(size_t i)
{
    const alarm_t *a = alarms_get(i);
    char buf[32];

    lv_obj_t *row = add_row(ROW_H);
    lv_obj_add_event_cb(row, on_row, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)i);

    lv_color_t color = a->enabled ? lv_color_white() : UI_COLOR_DIM;
    lv_obj_t *time = ui_label(row, &lv_font_montserrat_20, color);
    alarms_format_time(a, buf, sizeof(buf));
    lv_label_set_text(time, buf);
    lv_obj_align(time, LV_ALIGN_TOP_LEFT, 0, 7);

    lv_obj_t *days = ui_label(row, &lv_font_montserrat_14, UI_COLOR_DIM);
    alarms_format_days(a, buf, sizeof(buf));
    lv_label_set_text(days, buf);
    lv_obj_align(days, LV_ALIGN_BOTTOM_LEFT, 0, -7);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 40, 22);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, UI_COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_state(sw, LV_STATE_CHECKED, a->enabled);
    lv_obj_set_ext_click_area(sw, 10);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(sw, on_switch, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);
}

static void rebuild_list(void)
{
    lv_obj_clean(s_list);

    lv_obj_t *heading = ui_label(s_list, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(heading, LV_SYMBOL_BELL " Alarms");

    size_t n = alarms_count();
    if (n == 0) {
        lv_obj_t *empty = ui_label(s_list, &lv_font_montserrat_16, UI_COLOR_DIM);
        lv_label_set_text(empty, "No alarms yet");
    }
    for (size_t i = 0; i < n; i++) {
        add_alarm_row(i);
    }

    if (n < ALARMS_MAX) {
        lv_obj_t *add = add_row(44);
        lv_obj_add_event_cb(add, on_add, LV_EVENT_SHORT_CLICKED, NULL);
        lv_obj_t *label = ui_label(add, &lv_font_montserrat_16, UI_COLOR_ACCENT);
        lv_label_set_text(label, LV_SYMBOL_PLUS " Add alarm");
        lv_obj_center(label);
    }
}

static void on_list_loaded(lv_event_t *e)
{
    (void)e;
    // An alarm may have rung and turned itself off while another screen was showing.
    rebuild_list();
}

void alarm_app_create(lv_obj_t *parent)
{
    s_list = parent;
    lv_obj_set_scrollable(s_list, true);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(s_list, 16, 0);
    lv_obj_set_style_pad_bottom(s_list, 24, 0);
    lv_obj_set_style_pad_row(s_list, 8, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(s_list, on_list_loaded, LV_EVENT_SCREEN_LOADED, NULL);
}

void alarm_app_on_show(void)
{
    rebuild_list();
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
}

// ---- Editor ----

static void show_days(void)
{
    for (int d = 0; d < 7; d++) {
        bool on = s_edit_days & (1 << d);
        lv_obj_set_style_bg_color(s_days[d], on ? UI_COLOR_ACCENT : UI_COLOR_BUTTON, 0);
    }
}

static void on_day(lv_event_t *e)
{
    int d = (int)(uintptr_t)lv_event_get_user_data(e);
    s_edit_days ^= 1 << d;
    show_days();
}

static void on_save(lv_event_t *e)
{
    (void)e;
    alarm_t a = {.days = s_edit_days, .enabled = true};
    a.minute = (uint8_t)lv_roller_get_selected(s_minute);
    uint32_t hour = lv_roller_get_selected(s_hour);
    if (s_ampm) {
        // The hour wheel reads 12, 1, ... 11, so its position is the hour within the half day.
        hour += lv_roller_get_selected(s_ampm) * 12;
    }
    a.hour = (uint8_t)hour;

    if (s_editing == NEW_ALARM) {
        alarms_add(&a);
    } else {
        alarms_set(s_editing, &a);
    }
    rebuild_list();
    ui_close_overlay();
}

static void on_delete(lv_event_t *e)
{
    (void)e;
    alarms_remove(s_editing);
    rebuild_list();
    ui_close_overlay();
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

static void build_editor(const alarm_t *a)
{
    lv_obj_clean(s_edit);
    bool h24 = settings_get()->clock_24h;

    lv_obj_t *heading = ui_label(s_edit, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(heading, s_editing == NEW_ALARM ? "New alarm" : "Edit alarm");
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *wheels = lv_obj_create(s_edit);
    lv_obj_remove_style_all(wheels);
    lv_obj_set_size(wheels, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wheels, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wheels, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(wheels, 6, 0);
    lv_obj_set_scrollable(wheels, false);
    lv_obj_set_clickable(wheels, false);
    lv_obj_align(wheels, LV_ALIGN_TOP_MID, 0, 30);

    static char hours[24 * 3];
    static char minutes[60 * 3];
    char *p = hours;
    for (int h = 0; h < (h24 ? 24 : 12); h++) {
        int shown = h24 ? h : (h == 0 ? 12 : h);
        p += sprintf(p, h24 ? "%s%02d" : "%s%d", h ? "\n" : "", shown);
    }
    p = minutes;
    for (int m = 0; m < 60; m++) {
        p += sprintf(p, "%s%02d", m ? "\n" : "", m);
    }

    s_hour = add_roller(wheels, hours, LV_ROLLER_MODE_INFINITE, h24 ? a->hour : a->hour % 12);
    lv_obj_t *colon = ui_label(wheels, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(colon, ":");
    s_minute = add_roller(wheels, minutes, LV_ROLLER_MODE_INFINITE, a->minute);
    s_ampm = h24 ? NULL : add_roller(wheels, "AM\nPM", LV_ROLLER_MODE_NORMAL, a->hour >= 12);
    // After layout, so placing the wheels doesn't count as turning them.
    lv_obj_update_layout(s_edit);
    ui_roller_haptics(s_hour);
    ui_roller_haptics(s_minute);
    if (s_ampm) {
        ui_roller_haptics(s_ampm);
    }

    lv_obj_t *days = lv_obj_create(s_edit);
    lv_obj_remove_style_all(days);
    lv_obj_set_size(days, 220, DAY_SIZE);
    lv_obj_set_flex_flow(days, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(days, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(days, false);
    lv_obj_set_clickable(days, false);
    lv_obj_align(days, LV_ALIGN_TOP_MID, 0, 146);
    static const char *const LETTERS[] = {"S", "M", "T", "W", "T", "F", "S"};
    // Monday first, like the list.
    for (int k = 1; k <= 7; k++) {
        int d = k % 7;
        lv_obj_t *day = lv_obj_create(days);
        lv_obj_remove_style_all(day);
        lv_obj_set_size(day, DAY_SIZE, DAY_SIZE);
        lv_obj_set_style_radius(day, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(day, LV_OPA_COVER, 0);
        lv_obj_set_clickable(day, true);
        lv_obj_set_gesture_bubble(day, true);
        lv_obj_set_ext_click_area(day, 4);
        lv_obj_add_event_cb(day, on_day, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)d);
        lv_obj_t *letter = ui_label(day, &lv_font_montserrat_14, lv_color_white());
        lv_label_set_text(letter, LETTERS[d]);
        lv_obj_center(letter);
        s_days[d] = day;
    }
    s_edit_days = a->days;
    show_days();

    lv_obj_t *save = ui_round_button(s_edit, "Save", 48, UI_COLOR_ACCENT);
    lv_obj_set_gesture_bubble(save, true);
    lv_obj_add_event_cb(save, on_save, LV_EVENT_CLICKED, NULL);
    if (s_editing == NEW_ALARM) {
        lv_obj_set_width(save, 140);
        lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 0, -10);
    } else {
        lv_obj_set_width(save, 120);
        lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 32, -10);
        lv_obj_t *del = ui_round_button(s_edit, LV_SYMBOL_TRASH, 48, lv_palette_main(LV_PALETTE_RED));
        lv_obj_set_gesture_bubble(del, true);
        lv_obj_add_event_cb(del, on_delete, LV_EVENT_CLICKED, NULL);
        lv_obj_align(del, LV_ALIGN_BOTTOM_MID, -60, -10);
    }
}

static void open_editor(size_t index)
{
    if (!s_edit) {
        s_edit = ui_screen_create();
        ui_on_swipe(s_edit, LV_EVENT_GESTURE_RIGHT, ui_close_overlay);
    }
    s_editing = index;
    alarm_t a = {.hour = 7, .minute = 0, .days = 0};
    if (index != NEW_ALARM) {
        a = *alarms_get(index);
    }
    build_editor(&a);
    ui_show_overlay(s_edit);
}
