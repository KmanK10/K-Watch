#include "moon_icon.h"

#include <math.h>

#define LIT_R      0xEC
#define LIT_G      0xE8
#define LIT_B      0xDC
#define MARE_DARK  0.24f    // how much darker the maria are than the highlands
#define EARTHSHINE 0.17f    // brightness of the unlit side
#define TERMINATOR 0.05f    // width of the soft edge between day and night, in moon radii

typedef struct {
    float x, y, r;
} mare_t;

// The main maria as seen from the northern hemisphere, in moon radii from the centre (y down).
// Overlapping blobs, so the larger maria get irregular outlines.
static const mare_t MARIA[] = {
    {-0.30f, -0.45f, 0.26f},   // Imbrium
    {-0.12f, -0.55f, 0.16f},
    {-0.62f, -0.15f, 0.26f},   // Procellarum
    {-0.55f, 0.18f, 0.22f},
    {-0.38f, -0.02f, 0.16f},
    {0.17f, -0.38f, 0.15f},    // Serenitatis
    {0.05f, -0.62f, 0.10f},    // Frigoris
    {-0.25f, -0.72f, 0.09f},
    {0.30f, -0.12f, 0.17f},    // Tranquillitatis
    {0.18f, -0.02f, 0.10f},
    {0.68f, -0.30f, 0.10f},    // Crisium
    {0.55f, 0.12f, 0.12f},     // Fecunditatis
    {0.38f, 0.30f, 0.08f},     // Nectaris
    {-0.18f, 0.34f, 0.14f},    // Nubium
    {-0.46f, 0.42f, 0.09f},    // Humorum
};

static float clamp01(float v)
{
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

// The ESP32-S3's FPU has no divide, so the per-pixel code below sticks to multiplication:
// smoothstep takes 1 / (edge1 - edge0), and the maria are compared by squared distance.
static float smoothstep(float edge0, float inv_width, float x)
{
    float t = clamp01((x - edge0) * inv_width);
    return t * t * (3 - 2 * t);
}

#define MARIA_COUNT     (sizeof(MARIA) / sizeof(MARIA[0]))
#define MARE_INNER_SQ   (0.35f * 0.35f)    // fully dark inside this fraction of the radius...
#define MARE_OUTER_SQ   (1.25f * 1.25f)    // ...fading out by here (both squared)

// Surface brightness at (u, v): darker in the maria and towards the limb.
static float albedo(float u, float v, const float *inv_r_sq)
{
    static const float inv_fade = 1 / (MARE_OUTER_SQ - MARE_INNER_SQ);
    float dark = 0;
    for (size_t i = 0; i < MARIA_COUNT; i++) {
        float dx = u - MARIA[i].x;
        float dy = v - MARIA[i].y;
        float d_sq = (dx * dx + dy * dy) * inv_r_sq[i];
        if (d_sq >= MARE_OUTER_SQ) {
            continue;
        }
        float m = 1 - smoothstep(MARE_INNER_SQ, inv_fade, d_sq);
        dark = m > dark ? m : dark;
    }
    float limb = 0.82f + 0.18f * sqrtf(clamp01(1 - u * u - v * v));
    return (1 - MARE_DARK * dark) * limb;
}

static void on_delete(lv_event_t *e)
{
    lv_draw_buf_destroy(lv_event_get_user_data(e));
}

lv_obj_t *moon_icon_create(lv_obj_t *parent, int32_t size)
{
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_draw_buf_t *buf = lv_draw_buf_create((uint32_t)size, (uint32_t)size, LV_COLOR_FORMAT_ARGB8888, 0);
    lv_draw_buf_clear(buf, NULL);
    lv_canvas_set_draw_buf(canvas, buf);
    lv_obj_add_event_cb(canvas, on_delete, LV_EVENT_DELETE, buf);
    lv_obj_set_clickable(canvas, false);
    return canvas;
}

void moon_icon_set(lv_obj_t *icon, float phase, bool south)
{
    lv_draw_buf_t *buf = lv_canvas_get_draw_buf(icon);
    int32_t size = buf->header.w;
    float r = size / 2.0f;
    float inv_r = 1 / r;
    float cos_phase = cosf(phase * 3.14159265f / 180);
    bool waxing = phase <= 180;
    const float inv_terminator = 1 / (2 * TERMINATOR);
    float inv_r_sq[MARIA_COUNT];
    for (size_t i = 0; i < MARIA_COUNT; i++) {
        inv_r_sq[i] = 1 / (MARIA[i].r * MARIA[i].r);
    }
    // Each pixel averages a 2x2 grid of samples, which smooths the edges. The surface varies
    // slowly, so it's only worked out once per pixel.
    static const float SUB[2] = {0.25f, 0.75f};

    for (int32_t y = 0; y < size; y++) {
        uint8_t *row = buf->data + y * buf->header.stride;
        float v[2], v_sq[2], edge[2];
        for (int sy = 0; sy < 2; sy++) {
            v[sy] = (y + SUB[sy] - r) * inv_r;
            v_sq[sy] = v[sy] * v[sy];
            edge[sy] = sqrtf(clamp01(1 - v_sq[sy])) * cos_phase;   // where the terminator crosses this row
        }
        float cv = (y + 0.5f - r) * inv_r;

        for (int32_t x = 0; x < size; x++) {
            uint8_t *px = row + x * 4;
            float cu = (x + 0.5f - r) * inv_r;
            if (cu * cu + cv * cv > 1.1f) {
                px[0] = px[1] = px[2] = px[3] = 0;
                continue;
            }
            int cover = 0;
            float light = 0;
            for (int sx = 0; sx < 2; sx++) {
                float u = (x + SUB[sx] - r) * inv_r;
                float lu = south ? -u : u;
                for (int sy = 0; sy < 2; sy++) {
                    if (u * u + v_sq[sy] > 1) {
                        continue;
                    }
                    // Waxing moons are lit from the right in the north.
                    float d = waxing ? lu - edge[sy] : -edge[sy] - lu;
                    light += EARTHSHINE + (1 - EARTHSHINE) * smoothstep(-TERMINATOR, inv_terminator, d);
                    cover++;
                }
            }
            if (cover == 0) {
                px[0] = px[1] = px[2] = px[3] = 0;
                continue;
            }
            // The maria stay put on the moon's face, so flip them with it.
            float b = light * (cover == 4 ? 0.25f : 1.0f / cover) *
                      albedo(south ? -cu : cu, south ? -cv : cv, inv_r_sq);
            px[0] = (uint8_t)(LIT_B * b);
            px[1] = (uint8_t)(LIT_G * b);
            px[2] = (uint8_t)(LIT_R * b);
            px[3] = (uint8_t)(255 * cover / 4);
        }
    }
    lv_obj_invalidate(icon);
}
