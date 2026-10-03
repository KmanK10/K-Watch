#include "theme.h"

#include <stdint.h>

#include "esp_timer.h"
#include "haptics.h"

lv_obj_t *ui_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);
    return scr;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, "");
    return label;
}

void ui_label_max_lines(lv_obj_t *label, int lines)
{
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    int32_t line_h = lv_font_get_line_height(font);
    int32_t space = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);
    // Dots only appear once the text is taller than the label, so cap its height.
    lv_obj_set_style_max_height(label, lines * line_h + (lines - 1) * space, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
}

static void on_swipe(lv_event_t *e)
{
    void (*fn)(void) = (void (*)(void))lv_event_get_user_data(e);
    lv_indev_wait_release(lv_indev_active());
    fn();
}

void ui_on_swipe(lv_obj_t *screen, lv_event_code_t gesture, void (*fn)(void))
{
    lv_obj_add_event_cb(screen, on_swipe, gesture, (void *)fn);
}

// Fast flicks pass rows quicker than the motor can click; it just skips some.
#define TICK_MIN_US 35000

static int32_t roller_row(lv_obj_t *roller)
{
    const lv_font_t *font = lv_obj_get_style_text_font(roller, LV_PART_MAIN);
    int32_t row_h = lv_font_get_line_height(font) + lv_obj_get_style_text_line_space(roller, LV_PART_MAIN);
    int32_t y = lv_obj_get_y(lv_obj_get_child(roller, 0));
    // Round to the nearest row, so the click lands as a number reaches the middle.
    return (y >= 0 ? y + row_h / 2 : y - row_h / 2) / row_h;
}

// The roller moves its text label while dragged or coasting; every move is reported here.
static void on_roller_moved(lv_event_t *e)
{
    static int64_t last_tick_us;
    lv_obj_t *roller = lv_event_get_current_target(e);
    int32_t row = roller_row(roller);
    int32_t last = (int32_t)(intptr_t)lv_obj_get_user_data(roller);
    if (row == last) {
        return;
    }
    lv_obj_set_user_data(roller, (void *)(intptr_t)row);
    int64_t now = esp_timer_get_time();
    if (now - last_tick_us >= TICK_MIN_US) {
        last_tick_us = now;
        haptics_play(HAPTIC_TICK);
    }
}

void ui_roller_haptics(lv_obj_t *roller)
{
    lv_obj_set_user_data(roller, (void *)(intptr_t)roller_row(roller));
    lv_obj_add_event_cb(roller, on_roller_moved, LV_EVENT_CHILD_CHANGED, NULL);
}

// Anything nearly screen-sized is a background that reacts to taps anywhere, not a button.
static bool is_background(lv_obj_t *obj)
{
    int32_t w = lv_display_get_horizontal_resolution(NULL);
    int32_t h = lv_display_get_vertical_resolution(NULL);
    return lv_obj_get_width(obj) >= w * 4 / 5 && lv_obj_get_height(obj) >= h * 4 / 5;
}

// Every finished tap is reported to the input device. Long presses don't count, so dragging
// an app icon only gets its own feedback.
static void on_any_click(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_param(e);
    if (!obj || is_background(obj) || lv_obj_check_type(obj, &lv_roller_class)) {
        return;
    }
    haptics_play(HAPTIC_TAP);
}

void ui_click_haptics(void)
{
    for (lv_indev_t *indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
        lv_indev_add_event_cb(indev, on_any_click, LV_EVENT_SHORT_CLICKED, NULL);
    }
}

lv_obj_t *ui_round_button(lv_obj_t *parent, const char *text, int32_t size, lv_color_t bg)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *label = ui_label(btn, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}
