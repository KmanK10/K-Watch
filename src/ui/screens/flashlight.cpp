#include "../ui.hpp"

void initFlashlight(void) {
    LV_IMG_DECLARE(flashlight);

    ui_flashlight = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_flashlight, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ui_flashlightIcon = lv_img_create(ui_flashlight);
    lv_img_set_src(ui_flashlightIcon, &flashlight);
    lv_obj_align(ui_flashlightIcon, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_img_recolor_opa(ui_flashlightIcon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui_flashlightIcon, lv_color_hex(0xC8C8C8), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ui_indicator1 = lv_obj_create(ui_flashlight);
    lv_obj_align(ui_indicator1, LV_ALIGN_CENTER, -20, 30);
    lv_obj_set_size(ui_indicator1, 30, 15);
    lv_obj_set_style_bg_color(ui_indicator1, lv_color_hex(0xC8C8C8), LV_PART_MAIN | LV_STATE_DEFAULT);

    //lv_obj_t *ui_indicator2 = lv_obj_create(ui_flashlight);

    lv_obj_add_event_cb(ui_flashlight, [](lv_event_t *e){lv_obj_del(ui_flashlight);}, LV_EVENT_SCREEN_UNLOADED, NULL);
}