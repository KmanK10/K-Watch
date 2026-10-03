#include "glyph.h"

#include <stdbool.h>

#define SAMPLES 4   // per side, so each pixel averages 16 samples for smooth edges

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Shapes are drawn in a 1x1 box with y pointing down.
static bool in_moon(float x, float y)
{
    float dx = x - 0.5f, dy = y - 0.5f;
    if (dx * dx + dy * dy > 0.42f * 0.42f) {
        return false;
    }
    dx = x - 0.74f;
    dy = y - 0.32f;
    return dx * dx + dy * dy > 0.34f * 0.34f;
}

static bool in_lock(float x, float y)
{
    // Body: a rounded rectangle.
    float cx = clampf(x, 0.26f, 0.74f), cy = clampf(y, 0.54f, 0.84f);
    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 0.08f * 0.08f) {
        return true;
    }
    // Shackle: a half ring with short legs down into the body.
    if (y <= 0.42f) {
        float d = (x - 0.5f) * (x - 0.5f) + (y - 0.42f) * (y - 0.42f);
        return d <= 0.24f * 0.24f && d >= 0.14f * 0.14f;
    }
    return y < 0.5f && ((x >= 0.26f && x <= 0.36f) || (x >= 0.64f && x <= 0.74f));
}

static void on_delete(lv_event_t *e)
{
    lv_draw_buf_destroy(lv_event_get_user_data(e));
}

lv_obj_t *glyph_create(lv_obj_t *parent, glyph_t glyph, int32_t size, lv_color_t color)
{
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_draw_buf_t *buf = lv_draw_buf_create((uint32_t)size, (uint32_t)size, LV_COLOR_FORMAT_ARGB8888, 0);
    lv_canvas_set_draw_buf(canvas, buf);
    lv_obj_add_event_cb(canvas, on_delete, LV_EVENT_DELETE, buf);
    lv_obj_set_clickable(canvas, false);

    bool (*inside)(float, float) = glyph == GLYPH_MOON ? in_moon : in_lock;
    float step = 1.0f / ((float)size * SAMPLES);
    for (int32_t y = 0; y < size; y++) {
        uint8_t *row = buf->data + y * buf->header.stride;
        for (int32_t x = 0; x < size; x++) {
            int cover = 0;
            for (int sy = 0; sy < SAMPLES; sy++) {
                for (int sx = 0; sx < SAMPLES; sx++) {
                    cover += inside((x * SAMPLES + sx + 0.5f) * step, (y * SAMPLES + sy + 0.5f) * step);
                }
            }
            uint8_t *px = row + x * 4;
            px[0] = color.blue;
            px[1] = color.green;
            px[2] = color.red;
            px[3] = (uint8_t)(cover * 255 / (SAMPLES * SAMPLES));
        }
    }
    return canvas;
}
