#pragma once

// A rendered moon showing its phase, with the maria and a soft terminator.

#include <stdbool.h>

#include "lvgl.h"

lv_obj_t *moon_icon_create(lv_obj_t *parent, int32_t size);

// `phase` in degrees past new moon (see astro.h). From the southern hemisphere the moon
// appears flipped, lit from the other side.
void moon_icon_set(lv_obj_t *icon, float phase, bool south);
