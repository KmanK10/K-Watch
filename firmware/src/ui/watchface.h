#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "lvgl.h"

void watchface_create(lv_obj_t *parent);

// The setters only touch the screen when something visible changed.
void watchface_set_time(const struct tm *t);
void watchface_set_power(int battery_percent, bool charging, bool usb_connected);
void watchface_set_steps(uint32_t steps);
void watchface_set_connected(bool connected);
