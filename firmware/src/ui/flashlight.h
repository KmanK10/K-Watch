#pragma once

// A full-screen light: white, or red or green to keep night vision. Tap the light to
// show or hide the intensity and colour controls. Opened from the apps page; swipe
// right to turn it off.

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

void flashlight_create(lv_obj_t *parent);
void flashlight_on_show(void);
bool flashlight_is_on(void);

// Called with the backlight percent it wants while on, and 0 when it turns off for any
// reason (closed, the screen timed out, or an alert took over).
void flashlight_on_change(void (*cb)(uint8_t percent));
