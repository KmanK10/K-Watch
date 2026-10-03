#include "tint.h"

#include "lvgl.h"

static tint_t s_tint;

void tint_set(tint_t tint)
{
    if (tint == s_tint) {
        return;
    }
    s_tint = tint;
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
}

void tint_apply(uint16_t *px, uint32_t count)
{
    if (s_tint == TINT_NONE) {
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        uint32_t c = px[i];
        uint32_t r = c >> 11;            // 0-31
        uint32_t g = (c >> 5) & 0x3F;    // 0-63
        uint32_t b = c & 0x1F;           // 0-31
        // Perceived brightness on a 0-62 scale, weighted roughly 30/59/11 like a black-and-white TV.
        uint32_t y = (r * 154 + g * 150 + b * 58) >> 8;
        px[i] = s_tint == TINT_RED ? (uint16_t)((y >> 1) << 11) : (uint16_t)(y << 5);
    }
}
