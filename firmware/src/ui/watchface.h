#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "lvgl.h"
#include "weather.h"

void watchface_create(lv_obj_t *parent);

// The setters only touch the screen when something visible changed.
void watchface_set_time(const struct tm *t);
void watchface_set_power(int battery_percent, bool charging, bool usb_connected);
// Also fills the goal bar; it turns green once `steps` reaches `goal`.
void watchface_set_steps(uint32_t steps, uint32_t goal);
void watchface_set_connected(bool connected);
void watchface_set_24h(bool on);
void watchface_set_dnd(bool on);
// A padlock at the top while touches are locked.
void watchface_set_locked(bool locked);
// Briefly shows "Press button to unlock" in place of the step count.
void watchface_show_unlock_hint(void);
// Shows a bell while any alarm is on.
void watchface_set_alarm(bool on);
// The current conditions above the time; NULL hides them. Redraws every call, so only call it
// when the weather changes.
void watchface_set_weather(const weather_t *w);
