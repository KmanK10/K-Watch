#include "../ui.hpp"

void initHomeScreen() {
    ui_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_home, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_batPercent = lv_label_create(ui_home);
    lv_obj_set_x(ui_batPercent, 10);
    lv_obj_set_style_text_color(ui_batPercent, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_timeDigital = lv_label_create(ui_home);
    lv_obj_set_x(ui_timeDigital, 100);
    lv_obj_set_style_text_color(ui_timeDigital, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_date = lv_label_create(ui_home);
    lv_obj_set_x(ui_date, 160);
    lv_obj_set_style_text_color(ui_date, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *button = lv_btn_create(ui_home);
    lv_obj_center(button);
    lv_obj_set_size(button, 100, 20);
}