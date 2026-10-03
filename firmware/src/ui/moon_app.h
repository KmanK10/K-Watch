#pragma once

// The moon's phase, rise and set times, and the next full and new moons.
// Rise and set need the location from the companion app's weather.

#include "lvgl.h"

// The page is filled in by moon_app_on_show, which the apps page calls before every opening.
void moon_app_create(lv_obj_t *parent);
void moon_app_on_show(void);

// The current phase drawn into an apps-page button.
void moon_app_draw_icon(lv_obj_t *button);
