#pragma once

// Today's step count, plus the totals of the last 30 days. The motion sensor counts from zero
// at every boot, so the day's total is saved to flash and carried across restarts, and starts
// over at midnight.

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

#define STEPS_HISTORY_DAYS 30

void steps_init(void);
uint32_t steps_today(void);

// Starts a new day if midnight has passed. Cheap enough to call on every loop.
void steps_check_day(void);

// Ticks until the next midnight, so the main loop can wake up to start the new day.
TickType_t steps_ticks_until_midnight(void);

// Saves today's count. Without `force`, skips the write if one happened recently.
void steps_save(bool force);

// The total for a day: 0 is today so far, 1 is yesterday, up to STEPS_HISTORY_DAYS.
// False for days the watch didn't record, e.g. when it was off.
bool steps_on_day(int days_ago, uint32_t *count);

// Average over the last `days` finished days (not today), skipping days without data.
uint32_t steps_average(int days, int *days_with_data);

uint32_t steps_goal(void);
void steps_set_goal(uint32_t goal);
// Called after the goal changes.
void steps_on_goal_change(void (*cb)(void));
