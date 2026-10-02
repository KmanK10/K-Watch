#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "phone/phone.h"

// Now-playing screen with play/pause, skip and volume.
void music_create(lv_obj_t *parent);
void music_update(const phone_media_t *media);
void music_set_connected(bool connected);
