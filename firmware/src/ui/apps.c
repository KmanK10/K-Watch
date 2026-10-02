#include "apps.h"

#include <stdint.h>

#include "ui/countdown.h"
#include "ui/flashlight.h"
#include "ui/screens.h"
#include "ui/stopwatch.h"
#include "ui/theme.h"

#define ICON_SIZE   60
#define CELL_W      78
#define CONTENT_W   (3 * CELL_W)

typedef struct {
    const char *name;
    const char *icon;                    // an LV_SYMBOL_*
    uint32_t color;
    void (*create)(lv_obj_t *parent);    // builds the app into its own screen, the first time it opens
    void (*on_show)(void);               // optional, called each time it opens
    const char *screen;                  // instead of `create`: a swipe screen from the table in screens.c
} app_t;

static const app_t s_apps[] = {
    {"Flashlight", LV_SYMBOL_CHARGE, 0xFFC107, flashlight_create, flashlight_on_show, NULL},
    {"Timer", LV_SYMBOL_BELL, 0xFF5F1F, countdown_create, NULL, NULL},
    {"Stopwatch", LV_SYMBOL_LOOP, 0x2EBD59, stopwatch_create, stopwatch_on_show, NULL},
    {"Music", LV_SYMBOL_AUDIO, 0xE53950, NULL, NULL, "music"},
    {"Settings", LV_SYMBOL_SETTINGS, 0x707070, NULL, NULL, "settings"},
};
#define APP_COUNT (sizeof(s_apps) / sizeof(s_apps[0]))

static lv_obj_t *s_page;
static lv_obj_t *s_screens[APP_COUNT];

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

    lv_obj_t *btn = ui_round_button(cell, s_apps[i].icon, ICON_SIZE, lv_color_hex(s_apps[i].color));
    lv_obj_set_gesture_bubble(btn, true);
    lv_obj_add_event_cb(btn, on_app, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)i);

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

    lv_obj_t *grid = lv_obj_create(s_page);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, CONTENT_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid, 14, 0);
    lv_obj_set_scrollable(grid, false);
    lv_obj_set_clickable(grid, false);

    for (size_t i = 0; i < APP_COUNT; i++) {
        add_app(grid, i);
    }
}

void apps_on_show(void)
{
    lv_obj_scroll_to_y(s_page, 0, LV_ANIM_OFF);
}
