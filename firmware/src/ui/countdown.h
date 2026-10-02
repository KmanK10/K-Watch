#pragma once

// Countdown timer screen. The countdown keeps running while the screen is off;
// the main loop asks how long until it ends so it can wake up in time.

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "lvgl.h"

void countdown_create(lv_obj_t *parent);

TickType_t countdown_ticks_until_done(void);

// Returns true once when the countdown reaches zero, and resets the timer for reuse.
bool countdown_check_done(void);
