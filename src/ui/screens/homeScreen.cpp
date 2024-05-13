#include "../ui.hpp"

void initHomeScreen() {
    home = lv_obj_create(NULL);
    lv_obj_clear_flag( home, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
    lv_obj_add_event_cb(home, scr_unloaded_delete_cb, LV_EVENT_SCREEN_UNLOADED, &home);
    lv_obj_set_style_bg_color(home, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_bg_opa(home, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
}