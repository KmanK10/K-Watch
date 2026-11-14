#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    HAPTIC_TAP,      // one short click, touch feedback
    HAPTIC_TICK,     // the lightest click, for scroll wheels and steppers; touch feedback
    HAPTIC_NOTIFY,   // two buzzes
    HAPTIC_ALERT,    // long pulsing, for calls and alarms
} haptic_pattern_t;

esp_err_t haptics_init(void);

// Plays a pattern asynchronously and turns the motor off afterwards.
void haptics_play(haptic_pattern_t pattern);

// When off, the touch feedback patterns are skipped; notifications and alerts still play.
void haptics_set_touch_feedback(bool on);
