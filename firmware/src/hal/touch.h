#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t touch_init(void);

// Returns true and fills x/y while a finger is down. Skips the I2C read
// entirely when the interrupt line says nothing is touching.
bool touch_read(int16_t *x, int16_t *y);

// Monitor mode scans slowly but still raises the interrupt on a touch.
void touch_set_low_power(bool low_power);
