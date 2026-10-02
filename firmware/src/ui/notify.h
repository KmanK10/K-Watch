#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "phone/phone.h"

// Keeps the most recent notifications, mirroring what is on the phone.
void notify_add(const phone_notification_t *n);
void notify_remove(uint32_t uid);
void notify_clear(void);

// Pop-up card for one stored notification. Tap or swipe right to close it.
void notify_show_card(uint32_t uid);

// Scrollable list of stored notifications, for the screen grid.
void notify_list_create(lv_obj_t *parent);
void notify_list_on_show(void);
