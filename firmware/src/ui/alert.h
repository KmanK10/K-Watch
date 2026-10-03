#pragma once

// Full-screen ringing alert (timer done, alarm) that buzzes until it is stopped.

#include <stdbool.h>

// `on_stop` runs when the user taps Stop. With `on_snooze` set, a Snooze button
// appears too. Neither is called by alert_dismiss().
void alert_show(const char *title, const char *detail, void (*on_stop)(void), void (*on_snooze)(void));

// Silences and closes the alert without calling either callback.
void alert_dismiss(void);

bool alert_is_showing(void);
