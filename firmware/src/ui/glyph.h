#pragma once

// Small icons the built-in symbol font doesn't have.

#include "lvgl.h"

typedef enum {
    GLYPH_MOON,   // crescent, for sleep mode
    GLYPH_LOCK,   // padlock, for touch lock
} glyph_t;

lv_obj_t *glyph_create(lv_obj_t *parent, glyph_t glyph, int32_t size, lv_color_t color);
