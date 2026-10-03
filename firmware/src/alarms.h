#pragma once

// Wake-up alarms, kept in flash. The main loop sleeps until the next one is due.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#define ALARMS_MAX 6

typedef struct {
    uint8_t hour;       // 0-23
    uint8_t minute;
    uint8_t days;       // bit 0 Sunday ... bit 6 Saturday; 0 rings once, then turns itself off
    bool enabled;
} alarm_t;

void alarms_init(void);

size_t alarms_count(void);
const alarm_t *alarms_get(size_t i);
bool alarms_any_enabled(void);

// These save straight away. Add returns false when the list is full.
bool alarms_add(const alarm_t *a);
void alarms_set(size_t i, const alarm_t *a);
void alarms_remove(size_t i);

// Call after the clock is set, so the next alarm is worked out from the new time.
void alarms_time_changed(void);

TickType_t alarms_ticks_until_next(void);

// Returns true once when an alarm (or a snoozed one) is due, with its index in `which`.
bool alarms_check_due(size_t *which);

// Rings the alarm that just went off again in a few minutes. `automatic` is for when nobody
// answered it; those stop after a few tries so a forgotten alarm doesn't ring all day.
void alarms_snooze(bool automatic);
// The ringing alarm was stopped by the user.
void alarms_stop(void);

// "7:30 AM" or "07:30", following the 24-hour clock setting.
void alarms_format_time(const alarm_t *a, char *buf, size_t len);
// "Once", "Every day", "Weekdays", "Weekends" or e.g. "Mon Wed Fri".
void alarms_format_days(const alarm_t *a, char *buf, size_t len);
