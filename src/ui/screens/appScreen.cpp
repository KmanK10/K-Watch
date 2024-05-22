#include "../ui.hpp"

lv_obj_t *ui_appsList;
void createAppBtn(lv_obj_t *btn, const lv_img_dsc_t *img, int zoom, String name);

void initAppScreen(void) {
    ui_apps = lv_tileview_add_tile(ui_mainTV, 0, 0, LV_DIR_RIGHT);

    ui_appsList = lv_list_create(ui_apps);
    lv_obj_set_style_bg_color(ui_appsList, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_appsList, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(ui_appsList, 240, 240);
    lv_obj_set_style_pad_row(ui_appsList, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Flashlight
    LV_IMG_DECLARE(flashlight);
    lv_obj_t *ui_flashlightBtn = lv_btn_create(ui_appsList);
    createAppBtn(ui_flashlightBtn, &flashlight, 256, "Flashlight");
    lv_obj_add_event_cb(ui_flashlightBtn, [](lv_event_t *e){initFlashlight(); lv_scr_load(ui_flashlight);}, LV_EVENT_CLICKED, NULL);
}

void createAppBtn(lv_obj_t *btn, const lv_img_dsc_t* src, int zoom, String name) {
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF5F1F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(btn, 210, 50);

    lv_obj_t *icon = lv_img_create(btn);
    lv_img_set_src(icon, src);
    lv_img_set_zoom(icon, zoom);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 45, 0);
    lv_label_set_text(label, name.c_str());
}