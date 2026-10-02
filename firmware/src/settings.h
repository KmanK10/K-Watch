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
} settings_t;

void settings_init(void);
const settings_t *settings_get(void);

// Saves the new settings and tells the change listener.
void settings_update(const settings_t *s);
// Applies the new settings without saving, for live changes like dragging a slider.
// Call settings_update when done, or they are lost on restart.
void settings_preview(const settings_t *s);
void settings_on_change(void (*cb)(const settings_t *s));
