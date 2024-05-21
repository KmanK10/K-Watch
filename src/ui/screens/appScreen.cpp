#include "../ui.hpp"

void initAppScreen(void) {
    LV_IMG_DECLARE(flashlight);

    ui_apps = lv_tileview_add_tile(ui_mainTV, 0, 0, LV_DIR_RIGHT);

    lv_obj_t *ui_flashlightBtn = lv_btn_create(ui_apps);
    lv_obj_set_style_bg_color(ui_flashlightBtn, lv_color_hex(0xFF5F1F), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ui_flashlightIcon = lv_img_create(ui_flashlightBtn);
    lv_img_set_src(ui_flashlightIcon, &flashlight);
    lv_obj_center(ui_flashlightIcon);

    lv_obj_add_event_cb(ui_flashlightBtn, [](lv_event_t *e){initFlashlight(); lv_scr_load(ui_flashlight);}, LV_EVENT_CLICKED, NULL);
}