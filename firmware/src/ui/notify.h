#pragma once

#include <stdint.h>

#include "phone/phone.h"

// Keeps the most recent notifications, mirroring what is on the phone.
void notify_add(const phone_notification_t *n);
void notify_remove(uint32_t uid);
void notify_clear(void);

// Full-screen card for one stored notification. Tap or swipe right to go back.
void notify_show_card(uint32_t uid);
// Scrollable list of stored notifications. Swipe right to go back to the watch face.
void notify_show_list(void);
