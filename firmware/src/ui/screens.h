#pragma once

// Screen manager. The main screens sit on a grid around the watch face, and
// swiping drags the neighbouring screen in under the finger (let go early to cancel).
// To add a screen, write its create function and add one line to the table in screens.c.
//
// Pop-up screens that are not on the grid, such as a notification card or the
// pairing code, are shown as overlays that slide over the grid.

#include <stdbool.h>

#include "lvgl.h"

// Builds every screen and shows the watch face.
void ui_init(void);

// Back to the watch face, closing any overlay.
void ui_show_home(bool animate);

// Jumps to a grid screen by its name in the table, e.g. "music". False if there is none.
bool ui_show_screen(const char *name, bool animate);

void ui_show_overlay(lv_obj_t *screen);
// Slides back to the screen that was showing before this overlay opened.
void ui_close_overlay(void);
