#include "weather_icon.h"

#include "ui/theme.h"
#include "weather.h"

#define COLOR_SUN      lv_color_hex(0xFFC107)
#define COLOR_MOON     lv_color_hex(0xE0E6EA)
#define COLOR_CLOUD    lv_color_hex(0xCFD8DC)
#define COLOR_STORM    lv_color_hex(0x90A4AE)
#define COLOR_RAIN     lv_color_hex(0x42A5F5)
#define COLOR_SNOW     lv_color_white()

typedef enum {
    SKY_CLEAR,
    SKY_PARTLY,
    SKY_CLOUDY,
    SKY_FOG,
    SKY_DRIZZLE,
    SKY_RAIN,
    SKY_SNOW,
    SKY_THUNDER,
} sky_t;

static sky_t sky_for(uint8_t code)
{
    if (code == 0) return SKY_CLEAR;
    if (code <= 2) return SKY_PARTLY;
    if (code == 3) return SKY_CLOUDY;
    if (code == 45 || code == 48) return SKY_FOG;
    if (code >= 51 && code <= 57) return SKY_DRIZZLE;
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return SKY_RAIN;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return SKY_SNOW;
    if (code >= 95) return SKY_THUNDER;
    return SKY_CLOUDY;
}

// A filled shape with its top-left corner at (x, y). A radius of LV_RADIUS_CIRCLE makes circles.
static lv_obj_t *shape(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_color_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_clickable(o, false);
    return o;
}

static void sun(lv_obj_t *icon, int32_t x, int32_t y, int32_t d)
{
    lv_obj_t *s = shape(icon, x, y, d, d, COLOR_SUN);
    // A soft glow instead of rays, which would be fiddly at small sizes.
    lv_obj_set_style_shadow_color(s, COLOR_SUN, 0);
    lv_obj_set_style_shadow_width(s, d / 3, 0);
    lv_obj_set_style_shadow_opa(s, LV_OPA_50, 0);
}

static void moon(lv_obj_t *icon, int32_t x, int32_t y, int32_t d)
{
    shape(icon, x, y, d, d, COLOR_MOON);
    shape(icon, x + d * 2 / 5, y - d / 6, d * 4 / 5, d * 4 / 5, lv_color_black());
}

// A cloud `w` wide with its top-left at (x, y); it is about 0.62 * w tall.
static void cloud(lv_obj_t *icon, int32_t x, int32_t y, int32_t w, lv_color_t color)
{
    shape(icon, x, y + w * 30 / 100, w, w * 32 / 100, color);
    shape(icon, x + w * 32 / 100, y, w / 2, w / 2, color);
    shape(icon, x + w * 10 / 100, y + w * 18 / 100, w * 36 / 100, w * 36 / 100, color);
}

// Three marks under a raised cloud: rain streaks, drizzle dashes or snow dots.
static void precipitation(lv_obj_t *icon, int32_t s, sky_t sky)
{
    int32_t thick = s / 14 < 2 ? 2 : s / 14;
    for (int i = 0; i < 3; i++) {
        int32_t x = s * (28 + 22 * i) / 100;
        if (sky == SKY_SNOW) {
            int32_t d = s / 9 < 3 ? 3 : s / 9;
            shape(icon, x - d / 2, s * (i == 1 ? 80 : 70) / 100, d, d, COLOR_SNOW);
        } else {
            int32_t len = sky == SKY_DRIZZLE ? s * 12 / 100 : s * 22 / 100;
            shape(icon, x - thick / 2, s * 70 / 100, thick, len, COLOR_RAIN);
        }
    }
}

static const lv_font_t *bolt_font(int32_t s)
{
    if (s >= 80) return &lv_font_montserrat_48;
    if (s >= 48) return &lv_font_montserrat_32;
    if (s >= 30) return &lv_font_montserrat_20;
    return &lv_font_montserrat_14;
}

lv_obj_t *weather_icon_create(lv_obj_t *parent, int32_t size)
{
    lv_obj_t *icon = lv_obj_create(parent);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, size, size);
    lv_obj_set_scrollable(icon, false);
    lv_obj_set_clickable(icon, false);
    // The sun's glow reaches a little outside the box.
    lv_obj_set_overflow_visible(icon, true);
    return icon;
}

void weather_icon_set(lv_obj_t *icon, uint8_t code, bool day)
{
    lv_obj_clean(icon);
    int32_t s = lv_obj_get_style_width(icon, 0);
    sky_t sky = sky_for(code);

    switch (sky) {
    case SKY_CLEAR:
        if (day) {
            sun(icon, s / 5, s / 5, s * 3 / 5);
        } else {
            moon(icon, s / 5, s / 5, s * 3 / 5);
        }
        break;
    case SKY_PARTLY:
        if (day) {
            sun(icon, s / 10, s / 12, s / 2);
        } else {
            moon(icon, s / 10, s / 12, s / 2);
        }
        cloud(icon, s * 26 / 100, s * 40 / 100, s * 70 / 100, COLOR_CLOUD);
        break;
    case SKY_CLOUDY:
        cloud(icon, s * 8 / 100, s * 24 / 100, s * 84 / 100, COLOR_CLOUD);
        break;
    case SKY_FOG:
        for (int i = 0; i < 3; i++) {
            int32_t inset = i == 1 ? 0 : s / 10;
            shape(icon, s / 10 + inset, s * (30 + 18 * i) / 100, s * 8 / 10 - inset, s / 10, COLOR_STORM);
        }
        break;
    case SKY_DRIZZLE:
    case SKY_RAIN:
    case SKY_SNOW:
        cloud(icon, s * 8 / 100, s * 8 / 100, s * 84 / 100, COLOR_CLOUD);
        precipitation(icon, s, sky);
        break;
    case SKY_THUNDER: {
        cloud(icon, s * 8 / 100, s * 4 / 100, s * 84 / 100, COLOR_STORM);
        lv_obj_t *bolt = ui_label(icon, bolt_font(s), COLOR_SUN);
        lv_label_set_text(bolt, LV_SYMBOL_CHARGE);
        lv_obj_align(bolt, LV_ALIGN_BOTTOM_MID, 0, 0);
        break;
    }
    }
}

lv_obj_t *weather_temp_create(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_scrollable(box, false);
    lv_obj_set_clickable(box, false);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *number = ui_label(box, font, color);
    lv_label_set_text(number, "--");

    // The degree ring, sized to the font and sitting near the top of the digits.
    int32_t line_h = lv_font_get_line_height(font);
    int32_t d = line_h / 5 < 5 ? 5 : line_h / 5;
    lv_obj_t *ring = lv_obj_create(box);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, d, d);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(ring, color, 0);
    lv_obj_set_style_border_width(ring, d / 4 < 1 ? 1 : d / 4, 0);
    lv_obj_set_style_margin_top(ring, line_h / 6, 0);
    lv_obj_set_style_margin_left(ring, d / 4, 0);
    return box;
}

void weather_temp_set(lv_obj_t *temp, int16_t value)
{
    lv_obj_t *number = lv_obj_get_child(temp, 0);
    lv_obj_t *ring = lv_obj_get_child(temp, 1);
    if (value == WEATHER_UNKNOWN) {
        lv_label_set_text(number, "--");
        lv_obj_set_hidden(ring, true);
        return;
    }
    lv_label_set_text_fmt(number, "%d", value);
    lv_obj_set_hidden(ring, false);
}
