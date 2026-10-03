#include "apps.h"

#include <stdint.h>
#include <string.h>

#include "haptics.h"
#include "nvs.h"

#include "ui/alarm_app.h"
#include "ui/calculator.h"
#include "ui/countdown.h"
#include "ui/flashlight.h"
#include "ui/health_app.h"
#include "ui/screens.h"
#include "ui/stopwatch.h"
#include "ui/moon_app.h"
#include "ui/theme.h"
#include "ui/weather_app.h"
#include "ui/weather_icon.h"

#define ICON_SIZE   60
#define CELL_W      78
#define CONTENT_W   (3 * CELL_W)

typedef struct {
    const char *name;
    const char *icon;                    // an LV_SYMBOL_*, or "" when draw_icon draws it
    void (*draw_icon)(lv_obj_t *button);
    uint32_t color;
    void (*create)(lv_obj_t *parent);    // builds the app into its own screen, the first time it opens
    void (*on_show)(void);               // optional, called each time it opens
    const char *screen;                  // instead of `create`: a swipe screen from the table in screens.c
} app_t;

static void draw_weather_icon(lv_obj_t *button)
{
    lv_obj_t *icon = weather_icon_create(button, 40);
    weather_icon_set(icon, 2, true);
    lv_obj_center(icon);
}

static const app_t s_apps[] = {
    {"Flashlight", LV_SYMBOL_CHARGE, NULL, 0xFFC107, flashlight_create, flashlight_on_show, NULL},
    {"Alarms", LV_SYMBOL_BELL, NULL, 0x7E57C2, alarm_app_create, alarm_app_on_show, NULL},
    {"Timer", LV_SYMBOL_REFRESH, NULL, 0xFF5F1F, countdown_create, NULL, NULL},
    {"Stopwatch", LV_SYMBOL_LOOP, NULL, 0x2EBD59, stopwatch_create, stopwatch_on_show, NULL},
    {"Health", LV_SYMBOL_PLUS, NULL, 0xEC407A, health_app_create, health_app_on_show, NULL},
    {"Calculator", LV_SYMBOL_KEYBOARD, NULL, 0x1E88E5, calculator_create, NULL, NULL},
    {"Weather", "", draw_weather_icon, 0x29B6F6, weather_app_create, weather_app_on_show, NULL},
    {"Moon", "", moon_app_draw_icon, 0x283593, moon_app_create, moon_app_on_show, NULL},
    {"Music", LV_SYMBOL_AUDIO, NULL, 0xE53950, NULL, NULL, "music"},
    {"Settings", LV_SYMBOL_SETTINGS, NULL, 0x707070, NULL, NULL, "settings"},
};
#define APP_COUNT (sizeof(s_apps) / sizeof(s_apps[0]))

static lv_obj_t *s_page;
static lv_obj_t *s_grid;
static lv_obj_t *s_screens[APP_COUNT];

// ---- Saved order ----
// Stored as names, so apps added in a later update just appear at the end.

static size_t load_order(size_t order[APP_COUNT])
{
    size_t n = 0;
    bool placed[APP_COUNT] = {false};
    char names[APP_COUNT * 24];
    size_t len = sizeof(names);
    nvs_handle_t h;
    if (nvs_open("apps", NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_blob(h, "order", names, &len) == ESP_OK) {
            for (const char *p = names; p < names + len && *p; p += strlen(p) + 1) {
                for (size_t i = 0; i < APP_COUNT; i++) {
                    if (!placed[i] && strcmp(p, s_apps[i].name) == 0) {
                        placed[i] = true;
                        order[n++] = i;
                    }
                }
            }
        }
        nvs_close(h);
    }
    for (size_t i = 0; i < APP_COUNT; i++) {
        if (!placed[i]) {
            order[n++] = i;
        }
    }
    return n;
}

static void save_order(void)
{
    char names[APP_COUNT * 24];
    size_t len = 0;
    uint32_t cells = lv_obj_get_child_count(s_grid);
    for (uint32_t c = 0; c < cells; c++) {
        size_t i = (size_t)(uintptr_t)lv_obj_get_user_data(lv_obj_get_child(s_grid, c));
        size_t n = strlen(s_apps[i].name) + 1;
        if (len + n > sizeof(names)) {
            break;
        }
        memcpy(names + len, s_apps[i].name, n);
        len += n;
    }
    nvs_handle_t h;
    if (nvs_open("apps", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(h, "order", names, len) == ESP_OK) {
        nvs_commit(h);
    }
    nvs_close(h);
}

// ---- Rearranging ----
// Hold an icon until it clicks, then drag it; the others shift to make room.

static lv_obj_t *s_dragging;   // the cell being moved

static void on_hold(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    s_dragging = lv_obj_get_parent(btn);
    // Keep the finger on this icon instead of scrolling the page or swiping screens.
    lv_obj_set_scroll_chain_hor(btn, false);
    lv_obj_set_scroll_chain_ver(btn, false);
    lv_obj_set_style_outline_width(btn, 3, 0);
    lv_obj_set_style_transform_scale(btn, 280, 0);   // 256 is normal size
    haptics_play(HAPTIC_TAP);
}

static void on_drag(lv_event_t *e)
{
    (void)e;
    if (!s_dragging) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    uint32_t cells = lv_obj_get_child_count(s_grid);
    for (uint32_t c = 0; c < cells; c++) {
        lv_obj_t *cell = lv_obj_get_child(s_grid, c);
        lv_area_t a;
        lv_obj_get_coords(cell, &a);
        bool over = p.x >= a.x1 && p.x <= a.x2 && p.y >= a.y1 && p.y <= a.y2;
        if (cell != s_dragging && over) {
            lv_obj_move_to_index(s_dragging, (int32_t)c);
            haptics_play(HAPTIC_TICK);
            break;
        }
    }
}

static void on_drop(lv_event_t *e)
{
    if (!s_dragging) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_set_scroll_chain_hor(btn, true);
    lv_obj_set_scroll_chain_ver(btn, true);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_set_style_transform_scale(btn, 256, 0);
    s_dragging = NULL;
    save_order();
}

static void on_app(lv_event_t *e)
{
    size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    const app_t *app = &s_apps[i];
    if (app->screen) {
        ui_show_screen(app->screen, false);
        return;
    }
    if (!s_screens[i]) {
        s_screens[i] = ui_screen_create();
        app->create(s_screens[i]);
        ui_on_swipe(s_screens[i], LV_EVENT_GESTURE_RIGHT, ui_close_overlay);
    }
    if (app->on_show) {
        app->on_show();
    }
    ui_show_overlay(s_screens[i]);
}

static void add_app(lv_obj_t *grid, size_t i)
{
    lv_obj_t *cell = lv_obj_create(grid);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, CELL_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cell, 4, 0);
    lv_obj_set_scrollable(cell, false);
    lv_obj_set_clickable(cell, false);
    lv_obj_set_user_data(cell, (void *)(uintptr_t)i);

    lv_obj_t *btn = ui_round_button(cell, s_apps[i].icon, ICON_SIZE, lv_color_hex(s_apps[i].color));
    if (s_apps[i].draw_icon) {
        s_apps[i].draw_icon(btn);
    }
    lv_obj_set_gesture_bubble(btn, true);
    lv_obj_set_style_outline_color(btn, lv_color_white(), 0);
    lv_obj_set_style_transform_pivot_x(btn, ICON_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(btn, ICON_SIZE / 2, 0);
    lv_obj_add_event_cb(btn, on_app, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)i);
    lv_obj_add_event_cb(btn, on_hold, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(btn, on_drag, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(btn, on_drop, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn, on_drop, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_t *name = ui_label(cell, &lv_font_montserrat_14, lv_color_white());
    lv_label_set_text(name, s_apps[i].name);
}

void apps_create(lv_obj_t *parent)
{
    s_page = parent;
    lv_obj_set_scroll_dir(s_page, LV_DIR_VER);
    lv_obj_set_style_pad_top(s_page, 16, 0);
    lv_obj_set_style_pad_bottom(s_page, 16, 0);
    lv_obj_set_style_pad_row(s_page, 12, 0);
    lv_obj_set_flex_flow(s_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *heading = ui_label(s_page, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(heading, LV_SYMBOL_LIST " Apps");

    s_grid = lv_obj_create(s_page);
    lv_obj_remove_style_all(s_grid);
    lv_obj_set_size(s_grid, CONTENT_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_grid, 14, 0);
    lv_obj_set_scrollable(s_grid, false);
    lv_obj_set_clickable(s_grid, false);

    size_t order[APP_COUNT];
    size_t n = load_order(order);
    for (size_t k = 0; k < n; k++) {
        add_app(s_grid, order[k]);
    }

    lv_obj_t *hint = ui_label(s_page, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_label_set_text(hint, "Hold an icon to move it");
}

void apps_on_show(void)
{
    lv_obj_scroll_to_y(s_page, 0, LV_ANIM_OFF);
}
