#include "flashlight.h"

#include <stddef.h>

#include "nvs.h"
#include "ui/theme.h"

#define PANEL_W        216
#define SWATCH_SIZE    30
#define MIN_PERCENT    5

static const uint32_t COLORS[] = {0xFFFFFF, 0xFF0000, 0x00FF00};
#define COLOR_COUNT (sizeof(COLORS) / sizeof(COLORS[0]))

static lv_obj_t *s_screen;
static lv_obj_t *s_panel;
static lv_obj_t *s_slider;
static lv_obj_t *s_swatches[COLOR_COUNT];
static size_t s_color;
static uint8_t s_percent = 100;
static bool s_on;
static void (*s_on_change)(uint8_t percent);

// The colour and intensity are kept in flash, so the light comes back the way it was left.
typedef struct {
    uint8_t color;
    uint8_t percent;
} saved_t;

static saved_t s_saved;

static void load_state(void)
{
    nvs_handle_t h;
    if (nvs_open("flashlight", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    saved_t saved;
    size_t len = sizeof(saved);
    if (nvs_get_blob(h, "v", &saved, &len) == ESP_OK && len == sizeof(saved) && saved.color < COLOR_COUNT &&
        saved.percent >= MIN_PERCENT && saved.percent <= 100) {
        s_color = saved.color;
        s_percent = saved.percent;
    }
    nvs_close(h);
}

static void save_state(void)
{
    saved_t now = {.color = (uint8_t)s_color, .percent = s_percent};
    if (now.color == s_saved.color && now.percent == s_saved.percent) {
        return;
    }
    nvs_handle_t h;
    if (nvs_open("flashlight", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(h, "v", &now, sizeof(now)) == ESP_OK && nvs_commit(h) == ESP_OK) {
        s_saved = now;
    }
    nvs_close(h);
}

static void report(void)
{
    if (s_on_change) {
        s_on_change(s_on ? s_percent : 0);
    }
}

static void show_color(void)
{
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COLORS[s_color]), 0);
    for (size_t i = 0; i < COLOR_COUNT; i++) {
        lv_obj_set_style_outline_width(s_swatches[i], i == s_color ? 2 : 0, 0);
    }
}

static void on_swatch(lv_event_t *e)
{
    s_color = (size_t)(uintptr_t)lv_event_get_user_data(e);
    show_color();
}

static void on_slider(lv_event_t *e)
{
    (void)e;
    s_percent = (uint8_t)lv_slider_get_value(s_slider);
    report();
}

// Taps on the panel itself land on the panel, so only taps on the light reach here.
static void on_light_tap(lv_event_t *e)
{
    (void)e;
    lv_obj_set_hidden(s_panel, !lv_obj_is_hidden(s_panel));
}

static void on_unload(lv_event_t *e)
{
    (void)e;
    if (s_on) {
        s_on = false;
        report();
        save_state();
    }
}

static lv_obj_t *add_swatch(lv_obj_t *parent, size_t i)
{
    lv_obj_t *sw = lv_obj_create(parent);
    lv_obj_remove_style_all(sw);
    lv_obj_set_size(sw, SWATCH_SIZE, SWATCH_SIZE);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(COLORS[i]), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_color(sw, lv_color_white(), 0);
    lv_obj_set_style_outline_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_pad(sw, 3, 0);
    lv_obj_set_ext_click_area(sw, 8);
    lv_obj_set_clickable(sw, true);
    lv_obj_add_event_cb(sw, on_swatch, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)i);
    return sw;
}

void flashlight_create(lv_obj_t *parent)
{
    s_screen = parent;
    load_state();
    s_saved = (saved_t){.color = (uint8_t)s_color, .percent = s_percent};
    lv_obj_add_event_cb(s_screen, on_unload, LV_EVENT_SCREEN_UNLOAD_START, NULL);
    lv_obj_add_event_cb(s_screen, on_light_tap, LV_EVENT_SHORT_CLICKED, NULL);

    lv_obj_t *panel = lv_obj_create(s_screen);
    s_panel = panel;
    // Catches taps between the controls, so they don't hide the panel.
    lv_obj_set_clickable(panel, true);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, PANEL_W, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_70, 0);
    lv_obj_set_style_radius(panel, 18, 0);
    lv_obj_set_style_pad_hor(panel, 14, 0);
    lv_obj_set_style_pad_ver(panel, 12, 0);
    lv_obj_set_style_pad_row(panel, 6, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(panel, false);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -10);

    s_slider = lv_slider_create(panel);
    lv_slider_set_range(s_slider, MIN_PERCENT, 100);
    lv_slider_set_value(s_slider, s_percent, LV_ANIM_OFF);
    lv_obj_set_size(s_slider, PANEL_W - 28 - 16, 8);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider, 6, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s_slider, 14);
    // Dragging the slider sideways must not count as the swipe that closes the light.
    lv_obj_set_gesture_bubble(s_slider, false);
    lv_obj_add_event_cb(s_slider, on_slider, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *colors = lv_obj_create(panel);
    lv_obj_remove_style_all(colors);
    lv_obj_set_size(colors, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(colors, 5, 0);   // room for the selection ring
    lv_obj_set_flex_flow(colors, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(colors, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(colors, false);
    lv_obj_set_clickable(colors, false);
    for (size_t i = 0; i < COLOR_COUNT; i++) {
        s_swatches[i] = add_swatch(colors, i);
    }

    lv_obj_t *hint = ui_label(panel, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_label_set_text(hint, "Swipe right to turn off");

    show_color();
}

void flashlight_on_show(void)
{
    lv_obj_set_hidden(s_panel, true);
    s_on = true;
    report();
}

bool flashlight_is_on(void)
{
    return s_on;
}

void flashlight_on_change(void (*cb)(uint8_t percent))
{
    s_on_change = cb;
}
