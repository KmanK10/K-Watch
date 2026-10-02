#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "phone/phone.h"

// Full-screen card for one notification. Tapping it returns to the watch face.
void notify_show(const phone_notification_t *n);
bool notify_is_showing(void);
uint32_t notify_current_uid(void);
