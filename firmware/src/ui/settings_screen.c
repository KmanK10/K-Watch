#include "settings_screen.h"

#include <stddef.h>

#include "settings.h"
#include "ui/theme.h"

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
    ROW_RAISE_TO_WAKE,
    ROW_TAP_TO_WAKE,
    ROW_NOTIFY_VIBRATE,
    ROW_CLOCK_24H,
    ROW_BLUETOOTH,
    ROW_DND,
    ROW_COUNT,
} row_id_t;

static lv_obj_t *s_page;
static lv_obj_t *s_values[ROW_COUNT];   // value label or switch
static lv_obj_t *s_brightness_slider;
static lv_obj_t *s_forget_label;
static lv_obj_t *s_about;
static lv_timer_t *s_confirm_timer;
static void (*s_on_forget)(void);

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
    case ROW_RAISE_TO_WAKE: return &s->raise_to_wake;
    case ROW_TAP_TO_WAKE: return &s->tap_to_wake;
    case ROW_NOTIFY_VIBRATE: return &s->notify_vibrate;
    case ROW_CLOCK_24H: return &s->clock_24h;
    case ROW_BLUETOOTH: return &s->bluetooth;
    case ROW_DND: return &s->dnd;
    default: return NULL;
    }
}

static void refresh(void)
{
    settings_t s = *settings_get();
    lv_label_set_text_fmt(s_values[ROW_BRIGHTNESS], "%d%%", s.brightness);
    lv_slider_set_value(s_brightness_slider, s.brightness, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_values[ROW_TIMEOUT], "%d s", s.screen_timeout_s);
    for (row_id_t row = ROW_RAISE_TO_WAKE; row < ROW_COUNT; row++) {
        lv_obj_set_state(s_values[row], LV_STATE_CHECKED, *toggle_field(&s, row));
    }
}

static void on_row(lv_event_t *e)
{
    row_id_t row = (row_id_t)(uintptr_t)lv_event_get_user_data(e);
    settings_t s = *settings_get();
    if (row == ROW_TIMEOUT) {
        s.screen_timeout_s = next_step(TIMEOUT_STEPS, sizeof(TIMEOUT_STEPS), s.screen_timeout_s);
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

static void end_confirm(lv_timer_t *t)
{
    (void)t;
    if (s_confirm_timer) {
        lv_timer_delete(s_confirm_timer);
        s_confirm_timer = NULL;
    }
    lv_label_set_text(s_forget_label, "Forget iPhone");
}

static void on_forget(lv_event_t *e)
{
    (void)e;
    if (!s_confirm_timer) {
        lv_label_set_text(s_forget_label, "Tap again to forget");
        s_confirm_timer = lv_timer_create(end_confirm, CONFIRM_MS, NULL);
        return;
    }
    end_confirm(NULL);
    lv_label_set_text(s_forget_label, "Forgotten");
    if (s_on_forget) {
        s_on_forget();
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

void settings_screen_create(lv_obj_t *parent)
{
    s_page = parent;
    lv_obj_set_scroll_dir(s_page, LV_DIR_VER);
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
    add_toggle_row("24-hour clock", ROW_CLOCK_24H);

    lv_obj_t *forget = add_row("", on_forget, NULL);
    s_forget_label = lv_obj_get_child(forget, 0);
    lv_label_set_text(s_forget_label, "Forget iPhone");
    lv_obj_set_style_text_color(s_forget_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(s_forget_label, LV_ALIGN_CENTER, 0, 0);

    s_about = ui_label(s_page, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_obj_set_width(s_about, CONTENT_W);
    lv_obj_set_style_text_align(s_about, LV_TEXT_ALIGN_CENTER, 0);

    refresh();
}

void settings_screen_on_show(void)
{
    lv_obj_scroll_to_y(s_page, 0, LV_ANIM_OFF);
    refresh();
}

void settings_screen_set_about(const char *text)
{
    lv_label_set_text(s_about, text);
}

void settings_screen_on_forget(void (*cb)(void))
{
    s_on_forget = cb;
}
