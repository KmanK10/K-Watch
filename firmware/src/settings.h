#pragma once

// User settings, kept in flash.

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t brightness;          // percent
    uint8_t screen_timeout_s;
    bool raise_to_wake;
    bool tap_to_wake;
    bool notify_vibrate;         // timers and alarms always vibrate
    bool clock_24h;
    bool bluetooth;              // always on after a restart
    bool dnd;                    // notifications go quietly into the list; always off after a restart
    bool touch_feedback;         // clicks for taps, scroll wheels and dragging
    bool celsius;                // weather temperatures; Fahrenheit when false
    bool wind_kmh;               // weather wind speeds; mph when false
    bool touch_lock;             // touches only wake the watch face; always off after a restart
    bool sleep_mode;             // tinted, dim, touch locked, quiet notifications, no raise to wake
    bool sleep_green;            // sleep mode tint; red when false
    bool sleep_schedule;         // sleep mode turns on at sleep_start and off at sleep_end
    uint16_t sleep_start;        // minutes after midnight
    uint16_t sleep_end;
} settings_t;

void settings_init(void);
const settings_t *settings_get(void);

// Saves the new settings and tells the change listener.
void settings_update(const settings_t *s);
// Applies the new settings without saving, for live changes like dragging a slider.
// Call settings_update when done, or they are lost on restart.
void settings_preview(const settings_t *s);
void settings_on_change(void (*cb)(const settings_t *s));
