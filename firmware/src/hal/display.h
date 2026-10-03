#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

// Sets up the panel, backlight and LVGL (display + touch input).
// Requires pmu_init() and touch_init() to have run.
esp_err_t display_init(void);

// Panel asleep, backlight rail off, touch in monitor mode.
void display_sleep(void);
// Wakes the panel, draws a fresh frame, then turns the backlight on.
void display_wake(void);

// Takes effect now, or at the next display_wake if the screen is asleep.
void display_set_brightness(uint8_t percent);
// Drops the backlight to a third of the set brightness, without forgetting it.
void display_set_dimmed(bool dimmed);

// Runs LVGL timers. Returns how many ms until it wants to run again.
uint32_t display_run(void);

uint32_t display_inactive_ms(void);
void display_trigger_activity(void);

// While locked, touches never reach the UI.
void display_set_touch_locked(bool locked);
// True once for each touch that started while locked.
bool display_take_locked_touch(void);
