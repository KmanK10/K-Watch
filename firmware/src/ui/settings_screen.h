#pragma once

#include "lvgl.h"

void settings_screen_create(lv_obj_t *parent);
void settings_screen_on_show(void);

// Shown at the bottom of the page, e.g. the firmware version.
void settings_screen_set_about(const char *text);

// Called when the user confirms "Forget iPhone".
void settings_screen_on_forget(void (*cb)(void));
// Called when the user confirms "Factory reset".
void settings_screen_on_factory_reset(void (*cb)(void));
