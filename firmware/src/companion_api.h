#pragma once

// Handles messages from the companion app (docs/companion-protocol.md): weather, settings,
// health data, diagnostics and "find my watch". Runs on the main task.

void companion_api_init(void);

// One JSON message from PHONE_EVT_COMPANION. Doesn't free it.
void companion_api_handle(const char *json);

// Asks the app for fresh weather; `reason` is "stale" or "user".
void companion_api_request_weather(const char *reason);

// Tells the app a setting changed on the watch. Ignored while applying the app's own changes.
void companion_api_settings_changed(void);

// Called for the "find" message, to buzz and light up the watch.
void companion_api_on_find(void (*cb)(void));
