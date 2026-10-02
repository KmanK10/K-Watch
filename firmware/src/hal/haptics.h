#pragma once

#include "esp_err.h"

typedef enum {
    HAPTIC_TAP,      // one short click
    HAPTIC_NOTIFY,   // two buzzes
    HAPTIC_ALERT,    // long pulsing, for calls and alarms
} haptic_pattern_t;

esp_err_t haptics_init(void);

// Powers the DRV2605 up, plays the pattern, and powers it down again afterwards.
void haptics_play(haptic_pattern_t pattern);
