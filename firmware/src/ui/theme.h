#pragma once

#include "lvgl.h"

#define UI_COLOR_ACCENT  lv_color_hex(0xFF5F1F)
#define UI_COLOR_DIM     lv_palette_main(LV_PALETTE_GREY)
#define UI_COLOR_BUTTON  lv_color_hex(0x303030)

// Black screen, not scrollable unless the caller enables it.
lv_obj_t *ui_screen_create(void);

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color);

// Wraps up to `lines` lines, then ends with "..." (the label needs a width).
void ui_label_max_lines(lv_obj_t *label, int lines);

// Calls `fn` when the screen is swiped in a direction (LV_EVENT_GESTURE_LEFT etc.).
// The rest of that touch is ignored, so a swipe that starts on a button never presses it.
void ui_on_swipe(lv_obj_t *screen, lv_event_code_t gesture, void (*fn)(void));

// A light click from the motor each time a new value passes the middle of the wheel.
// Uses the roller's user data.
void ui_roller_haptics(lv_obj_t *roller);

// A click from the motor whenever a button, switch or row is tapped. Call once input devices exist.
void ui_click_haptics(void);

// Round button showing `text` (usually an LV_SYMBOL_*).
lv_obj_t *ui_round_button(lv_obj_t *parent, const char *text, int32_t size, lv_color_t bg);
