#include "settings_screen.h"

#include <stddef.h>
#include <stdio.h>

#include "settings.h"
#include "ui/theme.h"
#include "ui/time_picker.h"

#define CONTENT_W          200
#define ROW_H              44
#define CONFIRM_MS         3000

#define SLIDER_ROW_H       68
#define BRIGHTNESS_MIN     10
#define BRIGHTNESS_STEP    5

static const uint8_t TIMEOUT_STEPS[] = {5, 10, 15, 30};

typedef enum {
    ROW_BRIGHTNESS,
    ROW_TIMEOUT,
    ROW_SLEEP_COLOR,
    ROW_SLEEP_START,
    ROW_SLEEP_END,
    ROW_SLEEP_MODE,
    ROW_SLEEP_SCHEDULE,
    ROW_RAISE_TO_WAKE,
    ROW_TAP_TO_WAKE,
    ROW_NOTIFY_VIBRATE,
    ROW_CLOCK_24H,
    ROW_BLUETOOTH,
    ROW_DND,
    ROW_TOUCH_FEEDBACK,
    ROW_COUNT,
} row_id_t;

static lv_obj_t *s_page;
static lv_obj_t *s_values[ROW_COUNT];   // value label or switch
static lv_obj_t *s_brightness_slider;
static lv_obj_t *s_about;
static char s_about_text[96];

// A red row that needs a second tap within CONFIRM_MS before it acts.
typedef struct {
    const char *text;
    const char *confirm_text;
    const char *done_text;
    lv_obj_t *label;
    lv_timer_t *timer;
    void (*cb)(void);
} confirm_row_t;

static confirm_row_t s_forget = {
    .text = "Forget iPhone",
    .confirm_text = "Tap again to forget",
    .done_text = "Forgotten",
};
static confirm_row_t s_reset = {
    .text = "Factory reset",
    .confirm_text = "Tap again to erase all",
    .done_text = "Erasing...",
};

static uint8_t next_step(const uint8_t *steps, size_t count, uint8_t current)
{
    for (size_t i = 0; i < count; i++) {
        if (steps[i] > current) {
            return steps[i];
        }
    }
    return steps[0];
}

static bool *toggle_field(settings_t *s, row_id_t row)
{
    switch (row) {
    case ROW_SLEEP_MODE: return &s->sleep_mode;
    case ROW_SLEEP_SCHEDULE: return &s->sleep_schedule;
    case ROW_RAISE_TO_WAKE: return &s->raise_to_wake;
    case ROW_TAP_TO_WAKE: return &s->tap_to_wake;
    case ROW_NOTIFY_VIBRATE: return &s->notify_vibrate;
    case ROW_CLOCK_24H: return &s->clock_24h;
    case ROW_BLUETOOTH: return &s->bluetooth;
    case ROW_DND: return &s->dnd;
    case ROW_TOUCH_FEEDBACK: return &s->touch_feedback;
    default: return NULL;
    }
}

static void format_time(char *buf, size_t len, uint16_t minutes, bool h24)
{
    int hour = minutes / 60, minute = minutes % 60;
    if (h24) {
        snprintf(buf, len, "%02d:%02d", hour, minute);
    } else {
        snprintf(buf, len, "%d:%02d %s", hour % 12 == 0 ? 12 : hour % 12, minute, hour < 12 ? "AM" : "PM");
    }
}

static void refresh(void)
{
    settings_t s = *settings_get();
    lv_label_set_text_fmt(s_values[ROW_BRIGHTNESS], "%d%%", s.brightness);
    lv_slider_set_value(s_brightness_slider, s.brightness, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_values[ROW_TIMEOUT], "%d s", s.screen_timeout_s);
    lv_label_set_text(s_values[ROW_SLEEP_COLOR], s.sleep_green ? "Green" : "Red");
    char buf[16];
    format_time(buf, sizeof(buf), s.sleep_start, s.clock_24h);
    lv_label_set_text(s_values[ROW_SLEEP_START], buf);
    format_time(buf, sizeof(buf), s.sleep_end, s.clock_24h);
    lv_label_set_text(s_values[ROW_SLEEP_END], buf);
    lv_obj_set_hidden(lv_obj_get_parent(s_values[ROW_SLEEP_START]), !s.sleep_schedule);
    lv_obj_set_hidden(lv_obj_get_parent(s_values[ROW_SLEEP_END]), !s.sleep_schedule);
    for (row_id_t row = ROW_SLEEP_MODE; row < ROW_COUNT; row++) {
        lv_obj_set_state(s_values[row], LV_STATE_CHECKED, *toggle_field(&s, row));
    }
}

static void set_sleep_start(uint16_t minutes)
{
    settings_t s = *settings_get();
    s.sleep_start = minutes;
    settings_update(&s);
    refresh();
}

static void set_sleep_end(uint16_t minutes)
{
    settings_t s = *settings_get();
    s.sleep_end = minutes;
    settings_update(&s);
    refresh();
}

static void on_row(lv_event_t *e)
{
    row_id_t row = (row_id_t)(uintptr_t)lv_event_get_user_data(e);
    settings_t s = *settings_get();
    if (row == ROW_SLEEP_START) {
        time_picker_open("Sleep starts", s.sleep_start, set_sleep_start);
        return;
    }
    if (row == ROW_SLEEP_END) {
        time_picker_open("Sleep ends", s.sleep_end, set_sleep_end);
        return;
    }
    if (row == ROW_TIMEOUT) {
        s.screen_timeout_s = next_step(TIMEOUT_STEPS, sizeof(TIMEOUT_STEPS), s.screen_timeout_s);
    } else if (row == ROW_SLEEP_COLOR) {
        s.sleep_green = !s.sleep_green;
    } else {
        bool *field = toggle_field(&s, row);
        *field = !*field;
    }
    settings_update(&s);
    refresh();
}

// Brightness follows the finger, but is only written to flash on release.
static void on_brightness(lv_event_t *e)
{
    int32_t value = lv_slider_get_value(s_brightness_slider);
    value = (value + BRIGHTNESS_STEP / 2) / BRIGHTNESS_STEP * BRIGHTNESS_STEP;
    settings_t s = *settings_get();
    s.brightness = (uint8_t)value;
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        settings_preview(&s);
    } else {
        settings_update(&s);
    }
    lv_label_set_text_fmt(s_values[ROW_BRIGHTNESS], "%d%%", s.brightness);
}

static void end_confirm(confirm_row_t *c)
{
    if (c->timer) {
        lv_timer_delete(c->timer);
        c->timer = NULL;
    }
    lv_label_set_text(c->label, c->text);
}

static void on_confirm_timeout(lv_timer_t *t)
{
    end_confirm(lv_timer_get_user_data(t));
}

static void on_confirm_row(lv_event_t *e)
{
    confirm_row_t *c = lv_event_get_user_data(e);
    if (!c->timer) {
        lv_label_set_text(c->label, c->confirm_text);
        c->timer = lv_timer_create(on_confirm_timeout, CONFIRM_MS, c);
        return;
    }
    end_confirm(c);
    lv_label_set_text(c->label, c->done_text);
    if (c->cb) {
        c->cb();
    }
}

static lv_obj_t *add_row(const char *name, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *row = lv_obj_create(s_page);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, CONTENT_W, ROW_H);
    lv_obj_set_style_bg_color(row, UI_COLOR_BUTTON, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x484848), LV_STATE_PRESSED);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_clickable(row, true);
    lv_obj_set_gesture_bubble(row, true);
    lv_obj_set_scrollable(row, false);
    if (cb) {
        lv_obj_add_event_cb(row, cb, LV_EVENT_SHORT_CLICKED, user_data);
    }

    lv_obj_t *label = ui_label(row, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_text(label, name);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    return row;
}

static void add_value_row(const char *name, row_id_t id)
{
    lv_obj_t *row = add_row(name, on_row, (void *)(uintptr_t)id);
    s_values[id] = ui_label(row, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_obj_align(s_values[id], LV_ALIGN_RIGHT_MID, 0, 0);
}

static void add_brightness_row(void)
{
    lv_obj_t *row = add_row("Brightness", NULL, NULL);
    lv_obj_set_height(row, SLIDER_ROW_H);
    lv_obj_set_clickable(row, false);
    lv_obj_align(lv_obj_get_child(row, 0), LV_ALIGN_TOP_LEFT, 0, 10);

    s_values[ROW_BRIGHTNESS] = ui_label(row, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_obj_align(s_values[ROW_BRIGHTNESS], LV_ALIGN_TOP_RIGHT, 0, 10);

    lv_obj_t *slider = lv_slider_create(row);
    lv_slider_set_range(slider, BRIGHTNESS_MIN, 100);
    lv_obj_set_size(slider, CONTENT_W - 24 - 16, 8);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);
    // Easier to grab with a fingertip than the thin bar alone.
    lv_obj_set_ext_click_area(slider, 14);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_PRESS_LOST, NULL);
    s_brightness_slider = slider;
}

static void add_toggle_row(const char *name, row_id_t id)
{
    lv_obj_t *row = add_row(name, on_row, (void *)(uintptr_t)id);
    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 40, 22);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, UI_COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    // The whole row is the button; the switch only shows the state.
    lv_obj_set_clickable(sw, false);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    s_values[id] = sw;
}

static void add_confirm_row(confirm_row_t *c)
{
    lv_obj_t *row = add_row("", on_confirm_row, c);
    c->label = lv_obj_get_child(row, 0);
    lv_label_set_text(c->label, c->text);
    lv_obj_set_style_text_color(c->label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(c->label, LV_ALIGN_CENTER, 0, 0);
}

void settings_screen_create(lv_obj_t *parent)
{
    s_page = parent;
    lv_obj_set_scrollable(s_page, true);
    lv_obj_set_scroll_dir(s_page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(s_page, 16, 0);
    lv_obj_set_style_pad_bottom(s_page, 24, 0);
    lv_obj_set_style_pad_row(s_page, 8, 0);
    lv_obj_set_flex_flow(s_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *heading = ui_label(s_page, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(heading, LV_SYMBOL_SETTINGS " Settings");

    add_toggle_row("Do not disturb", ROW_DND);
    add_toggle_row("Bluetooth", ROW_BLUETOOTH);
    add_brightness_row();
    add_value_row("Screen timeout", ROW_TIMEOUT);
    add_toggle_row("Raise to wake", ROW_RAISE_TO_WAKE);
    add_toggle_row("Tap to wake", ROW_TAP_TO_WAKE);
    add_toggle_row("Buzz on notify", ROW_NOTIFY_VIBRATE);
    add_toggle_row("Touch feedback", ROW_TOUCH_FEEDBACK);
    add_toggle_row("24-hour clock", ROW_CLOCK_24H);

    lv_obj_t *sleep_heading = ui_label(s_page, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_label_set_text(sleep_heading, "Sleep");
    lv_obj_set_width(sleep_heading, CONTENT_W - 24);
    lv_obj_set_style_pad_top(sleep_heading, 8, 0);
    add_toggle_row("Sleep mode", ROW_SLEEP_MODE);
    add_value_row("Colour", ROW_SLEEP_COLOR);
    add_toggle_row("Schedule", ROW_SLEEP_SCHEDULE);
    add_value_row("Starts", ROW_SLEEP_START);
    add_value_row("Ends", ROW_SLEEP_END);

    add_confirm_row(&s_forget);
    add_confirm_row(&s_reset);

    s_about = ui_label(s_page, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_obj_set_width(s_about, CONTENT_W);
    lv_obj_set_style_text_align(s_about, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_about, s_about_text);

    refresh();
}

void settings_screen_on_show(void)
{
    lv_obj_scroll_to_y(s_page, 0, LV_ANIM_OFF);
    refresh();
}

void settings_screen_set_about(const char *text)
{
    snprintf(s_about_text, sizeof(s_about_text), "%s", text);
    if (s_about) {
        lv_label_set_text(s_about, s_about_text);
    }
}

void settings_screen_on_forget(void (*cb)(void))
{
    s_forget.cb = cb;
}

void settings_screen_on_factory_reset(void (*cb)(void))
{
    s_reset.cb = cb;
}
