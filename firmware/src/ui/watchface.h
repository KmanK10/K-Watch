#pragma once

#include <stdbool.h>
#include <time.h>

void watchface_create(void);

// Both setters only touch the screen when something visible changed.
void watchface_set_time(const struct tm *t);
void watchface_set_power(int battery_percent, bool charging, bool usb_connected);
