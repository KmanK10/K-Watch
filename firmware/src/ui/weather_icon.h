#pragma once

// Weather icons drawn from simple shapes (sun, moon, cloud, rain, snow, lightning, fog), so they
// scale to any size without image files. Meant for black backgrounds: the moon's crescent is cut
// out with a black circle.

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

lv_obj_t *weather_icon_create(lv_obj_t *parent, int32_t size);
// `code` is a WMO weather code; `day` false shows a moon instead of a sun.
void weather_icon_set(lv_obj_t *icon, uint8_t code, bool day);

// A temperature with a degree sign (the font has no °). Shows "--" for WEATHER_UNKNOWN.
lv_obj_t *weather_temp_create(lv_obj_t *parent, const lv_font_t *font, lv_color_t color);
void weather_temp_set(lv_obj_t *temp, int16_t value);
