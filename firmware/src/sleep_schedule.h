#pragma once

// Turns sleep mode on and off at the scheduled times. Sleep mode can still be switched by hand
// in between; the schedule only acts when a start or end time passes.

#include <stdint.h>

// Call often (each pass of the main loop). Right after boot it sets sleep mode to match the schedule.
void sleep_schedule_check(void);

// Seconds until the next scheduled start or end, or UINT32_MAX with no schedule.
uint32_t sleep_schedule_seconds_until_next(void);
