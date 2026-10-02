#pragma once

// Today's step count. The motion sensor counts from zero at every boot, so the day's
// total is saved to flash and carried across restarts, and starts over at midnight.

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

void steps_init(void);
uint32_t steps_today(void);

// Starts a new day if midnight has passed. Cheap enough to call on every loop.
void steps_check_day(void);

// Ticks until the next midnight, so the main loop can wake up to start the new day.
TickType_t steps_ticks_until_midnight(void);

// Saves today's count. Without `force`, skips the write if one happened recently.
void steps_save(bool force);
