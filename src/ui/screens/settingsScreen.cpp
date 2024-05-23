#include "../ui.hpp"

void initSettingsScreen(void) {
    ui_settings = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_settings, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_settings, [](lv_event_t *e){lv_obj_del(ui_settings);}, LV_EVENT_SCREEN_UNLOADED, NULL);
}