#include "../ui.hpp"

void initHomeScreen() {
    LV_IMG_DECLARE(battery);

    ui_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_home, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_batIcon = lv_img_create(ui_home);
    lv_img_set_src(ui_batIcon, &battery);
    lv_obj_align(ui_batIcon, LV_ALIGN_TOP_RIGHT, 0, 0);

    ui_batBar = lv_bar_create(ui_batIcon);
    lv_obj_align(ui_batBar, LV_ALIGN_TOP_RIGHT, -6, 2);
    lv_obj_set_style_bg_color(ui_batBar, lv_color_white(), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_batBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_batBar, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_width(ui_batBar, 32, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_height(ui_batBar, 14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_batPercent = lv_label_create(ui_batIcon);
    lv_obj_align(ui_batPercent, LV_ALIGN_CENTER, -2, 0);
    lv_obj_set_style_text_color(ui_batPercent, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_batPercent, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_date = lv_label_create(ui_home);
    lv_obj_align(ui_date, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(ui_date, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_date, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_timeDigital = lv_label_create(ui_home);
    lv_obj_align(ui_timeDigital, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(ui_timeDigital, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_timeDigital, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_home, [](lv_event_t *e){lv_obj_del(ui_home);}, LV_EVENT_SCREEN_UNLOADED, NULL); // Delete home screen on unloaded
}