#include "../ui.hpp"

void initMainScreen() {
    // Set refresh flags
    info.flag.refreshBatColor = true;
    info.flag.refresh = true;

    ui_main = lv_obj_create(NULL);

    ui_mainTV = lv_tileview_create(ui_main);
    lv_obj_set_style_bg_color(ui_mainTV, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ui_mainTV, LV_SCROLLBAR_MODE_OFF);

    // Init tiles
    initHomeScreen();
    initAppScreen();

    lv_obj_set_tile(ui_mainTV, ui_home, LV_ANIM_OFF); // Scroll to home

    lv_obj_add_event_cb(ui_main, [](lv_event_t *e){lv_obj_del(ui_main);}, LV_EVENT_SCREEN_UNLOADED, NULL);
}