#include "quick_settings.h"

#include "settings.h"
#include "ui/apps.h"
#include "ui/glyph.h"
#include "ui/theme.h"
#include "ui/weather_icon.h"

#define MARGIN          8
#define GAP             8
#define TILE_W          ((240 - 2 * MARGIN - GAP) / 2)
#define TILE_H          60
#define GRID_Y          12
#define SLIDER_Y        (GRID_Y + 2 * TILE_H + 2 * GAP + 4)
#define SLIDER_H        44
#define PILLS_Y         (SLIDER_Y + SLIDER_H + 14)
#define PILL_H          48
#define BOTTOM_PAD      16
#define CONFIRM_MS      3000
#define BRIGHTNESS_MIN  10
#define BRIGHTNESS_STEP 5

#define COLOR_OFF       lv_color_hex(0x3A3A3A)
#define COLOR_PRESSED   lv_color_hex(0x484848)

typedef enum {
    TOGGLE_DND,
    TOGGLE_SLEEP,
    TOGGLE_TOUCH_LOCK,
    TOGGLE_COUNT,
} toggle_id_t;

static const uint32_t ON_COLORS[TOGGLE_COUNT] = {
    [TOGGLE_DND] = 0x7E57C2,
    [TOGGLE_SLEEP] = 0x3949AB,
    [TOGGLE_TOUCH_LOCK] = 0x00897B,
};

static lv_obj_t *s_page;
static lv_obj_t *s_toggles[TOGGLE_COUNT];
static lv_obj_t *s_slider;
static lv_obj_t *s_power_label;
static lv_timer_t *s_power_timer;
static void (*s_on_power_off)(void);

static bool toggle_value(const settings_t *s, toggle_id_t id)
{
    switch (id) {
    case TOGGLE_DND: return s->dnd;
    case TOGGLE_SLEEP: return s->sleep_mode;
    case TOGGLE_TOUCH_LOCK: return s->touch_lock;
    default: return false;
    }
}

static void refresh(void)
{
    const settings_t *s = settings_get();
    for (toggle_id_t id = 0; id < TOGGLE_COUNT; id++) {
        bool on = toggle_value(s, id);
        lv_obj_set_style_bg_color(s_toggles[id], on ? lv_color_hex(ON_COLORS[id]) : COLOR_OFF, 0);
    }
    lv_slider_set_value(s_slider, s->brightness, LV_ANIM_OFF);
}

static void on_toggle(lv_event_t *e)
{
    toggle_id_t id = (toggle_id_t)(uintptr_t)lv_event_get_user_data(e);
    settings_t s = *settings_get();
    switch (id) {
    case TOGGLE_DND: s.dnd = !s.dnd; break;
    case TOGGLE_SLEEP: s.sleep_mode = !s.sleep_mode; break;
    case TOGGLE_TOUCH_LOCK: s.touch_lock = !s.touch_lock; break;
    default: break;
    }
    settings_update(&s);
    refresh();
}

static void on_flashlight(lv_event_t *e)
{
    (void)e;
    apps_open("Flashlight");
}

static void on_settings(lv_event_t *e)
{
    (void)e;
    apps_open("Settings");
}

// Brightness follows the finger, but is only written to flash on release.
static void on_brightness(lv_event_t *e)
{
    int32_t value = lv_slider_get_value(s_slider);
    value = (value + BRIGHTNESS_STEP / 2) / BRIGHTNESS_STEP * BRIGHTNESS_STEP;
    settings_t s = *settings_get();
    s.brightness = (uint8_t)value;
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        settings_preview(&s);
    } else {
        settings_update(&s);
    }
}

static void end_power_confirm(void)
{
    if (s_power_timer) {
        lv_timer_delete(s_power_timer);
        s_power_timer = NULL;
    }
    lv_label_set_text(s_power_label, LV_SYMBOL_POWER " Off");
}

static void on_power_timeout(lv_timer_t *t)
{
    (void)t;
    end_power_confirm();
}

// Needs a second tap within CONFIRM_MS, like the red rows in Settings.
static void on_power(lv_event_t *e)
{
    (void)e;
    if (!s_power_timer) {
        lv_label_set_text(s_power_label, "Tap again");
        s_power_timer = lv_timer_create(on_power_timeout, CONFIRM_MS, NULL);
        return;
    }
    end_power_confirm();
    lv_label_set_text(s_power_label, "Bye");
    if (s_on_power_off) {
        s_on_power_off();
    }
}

static lv_obj_t *add_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, lv_event_cb_t cb,
                            void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, 18, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, COLOR_OFF, 0);
    lv_obj_set_style_bg_color(btn, COLOR_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_gesture_bubble(btn, true);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}

// A grid tile: icon on the left, name beside it. With no symbol, the caller adds the icon.
static lv_obj_t *add_tile(lv_obj_t *parent, int col, int row, const char *symbol, const char *name,
                          lv_color_t icon_color, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = add_button(parent, MARGIN + col * (TILE_W + GAP), GRID_Y + row * (TILE_H + GAP), TILE_W,
                               TILE_H, cb, user_data);
    if (symbol) {
        lv_obj_t *icon = ui_label(btn, &lv_font_montserrat_20, icon_color);
        lv_label_set_text(icon, symbol);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);
    }
    lv_obj_t *label = ui_label(btn, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_text(label, name);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 46, 0);
    return btn;
}

static lv_obj_t *add_pill(lv_obj_t *parent, int col, const char *text, lv_color_t text_color, lv_event_cb_t cb)
{
    lv_obj_t *btn = add_button(parent, MARGIN + col * (TILE_W + GAP), PILLS_Y, TILE_W, PILL_H, cb, NULL);
    lv_obj_set_style_radius(btn, PILL_H / 2, 0);
    lv_obj_t *label = ui_label(btn, &lv_font_montserrat_16, text_color);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return label;
}

static void add_brightness(lv_obj_t *parent)
{
    s_slider = lv_slider_create(parent);
    lv_obj_set_size(s_slider, 2 * TILE_W + GAP, SLIDER_H);
    lv_obj_set_pos(s_slider, MARGIN, SLIDER_Y);
    lv_slider_set_range(s_slider, BRIGHTNESS_MIN, 100);
    lv_obj_set_style_radius(s_slider, 18, LV_PART_MAIN);
    lv_obj_set_style_radius(s_slider, 18, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, COLOR_OFF, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0xE8E8E8), LV_PART_INDICATOR);
    // A thick bar you drag anywhere on, like Control Center; no knob.
    lv_obj_set_style_bg_opa(s_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_slider, on_brightness, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_slider, on_brightness, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_t *sun = weather_icon_create(s_slider, 22);
    weather_icon_set(sun, 0, true);
    lv_obj_align(sun, LV_ALIGN_LEFT_MID, 14, 0);
}

void quick_settings_create(lv_obj_t *parent)
{
    // Scrolls down to the More and Off pills; at the bottom, swiping up carries on to the watch face.
    s_page = parent;
    lv_obj_set_scrollable(parent, true);
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(parent, BOTTOM_PAD, 0);

    s_toggles[TOGGLE_DND] = add_tile(parent, 0, 0, LV_SYMBOL_EYE_CLOSE, "DND", lv_color_white(), on_toggle,
                                     (void *)(uintptr_t)TOGGLE_DND);
    s_toggles[TOGGLE_SLEEP] =
        add_tile(parent, 1, 0, NULL, "Sleep", lv_color_white(), on_toggle, (void *)(uintptr_t)TOGGLE_SLEEP);
    lv_obj_align(glyph_create(s_toggles[TOGGLE_SLEEP], GLYPH_MOON, 22, lv_color_white()), LV_ALIGN_LEFT_MID, 14,
                 0);
    add_tile(parent, 0, 1, LV_SYMBOL_CHARGE, "Light", lv_color_hex(0xFFC107), on_flashlight, NULL);
    s_toggles[TOGGLE_TOUCH_LOCK] =
        add_tile(parent, 1, 1, NULL, "Lock", lv_color_white(), on_toggle, (void *)(uintptr_t)TOGGLE_TOUCH_LOCK);
    lv_obj_align(glyph_create(s_toggles[TOGGLE_TOUCH_LOCK], GLYPH_LOCK, 22, lv_color_white()), LV_ALIGN_LEFT_MID,
                 14, 0);

    add_brightness(parent);

    add_pill(parent, 0, LV_SYMBOL_SETTINGS " More", lv_color_white(), on_settings);
    s_power_label = add_pill(parent, 1, LV_SYMBOL_POWER " Off", lv_palette_main(LV_PALETTE_RED), on_power);

    refresh();
}

void quick_settings_on_show(void)
{
    refresh();
    lv_obj_scroll_to_y(s_page, 0, LV_ANIM_OFF);
    if (s_power_timer) {
        end_power_confirm();
    }
}

void quick_settings_on_power_off(void (*cb)(void))
{
    s_on_power_off = cb;
}
