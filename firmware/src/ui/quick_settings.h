#pragma once

// The page above the watch face: a few toggles, brightness, and shortcuts to the flashlight,
// the full Settings app and power off.

#include "lvgl.h"

void quick_settings_create(lv_obj_t *parent);
void quick_settings_on_show(void);

// Called when the user confirms "Power off".
void quick_settings_on_power_off(void (*cb)(void));
