#pragma once

#include <stdbool.h>

#include "phone/phone.h"

// Now-playing screen with play/pause, skip and volume. Swipe right to go back.
void music_show(void);
void music_update(const phone_media_t *media);
void music_set_connected(bool connected);
