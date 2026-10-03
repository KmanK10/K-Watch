#pragma once

// The companion app's GATT service (see docs/companion-protocol.md): JSON messages split into
// chunks over an RX and a TX characteristic. Internal to the phone module; the rest of the
// firmware uses phone_companion_send and PHONE_EVT_COMPANION.

#include <stdbool.h>

#include "host/ble_hs.h"

extern const ble_uuid128_t COMPANION_SVC_UUID;

// Adds the service. Call before the Bluetooth host starts. `on_message` runs on the Bluetooth
// task and takes ownership of the malloc'd, NUL-terminated message.
void companion_register(void (*on_message)(char *json));

// For the GAP events the service cares about (subscribe, disconnect, notify done).
void companion_gap_event(const struct ble_gap_event *ev);

// Any task. Copies the message; false if the app isn't listening or the queue is full.
bool companion_send(const char *json);

// True while the app is subscribed to TX.
bool companion_ready(void);
