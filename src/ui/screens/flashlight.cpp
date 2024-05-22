#include "../ui.hpp"

lv_obj_t *ui_onOffBtn;
lv_obj_t *ui_indicator1;
lv_obj_t *ui_indicator2;

bool flashlightOn;
bool flashlightNight;

void initFlashlight(void) {
    watch.setBrightness(100);

    LV_IMG_DECLARE(flashlight);

    flashlightOn = true;
    flashlightNight = false;

    ui_flashlight = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_flashlight, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ui_flashlightIcon = lv_img_create(ui_flashlight);
    lv_img_set_src(ui_flashlightIcon, &flashlight);
    lv_obj_align(ui_flashlightIcon, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_img_recolor_opa(ui_flashlightIcon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui_flashlightIcon, lv_color_hex(0xBBBBBB), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_indicator1 = lv_obj_create(ui_flashlight);
    lv_obj_align(ui_indicator1, LV_ALIGN_CENTER, -12, 20);
    lv_obj_set_size(ui_indicator1, 20, 10);
    lv_obj_set_style_bg_color(ui_indicator1, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ui_indicator1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_opa(ui_indicator1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    

    ui_indicator2 = lv_obj_create(ui_flashlight);
    lv_obj_align(ui_indicator2, LV_ALIGN_CENTER, 12, 20);
    lv_obj_set_size(ui_indicator2, 20, 10);
    lv_obj_set_style_bg_color(ui_indicator2, lv_color_hex(0xBBBBBB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ui_indicator2, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_opa(ui_indicator2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_onOffBtn = lv_btn_create(ui_flashlight);
    lv_obj_center(ui_onOffBtn);
    lv_obj_set_size(ui_onOffBtn, 75, 75);
    lv_obj_set_style_opa(ui_onOffBtn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Events
    // Click
    lv_obj_add_event_cb(ui_onOffBtn, [](lv_event_t *e) {
        // Toggle flashlight
        if (flashlightOn) {
            flashlightOn = false;
            lv_obj_set_style_bg_color(ui_flashlight, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (flashlightNight) {
            flashlightOn = true;
            lv_obj_set_style_bg_color(ui_flashlight, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            flashlightOn = true;
            lv_obj_set_style_bg_color(ui_flashlight, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }, LV_EVENT_CLICKED, NULL);

    // Gesture
    lv_obj_add_event_cb(ui_flashlight, [](lv_event_t *e) {
        // Swipped horizontally
        if (lv_indev_get_gesture_dir(lv_indev_get_act()) == 1 || lv_indev_get_gesture_dir(lv_indev_get_act()) == 2) {
            // Switch colors
            if (flashlightNight) {
                lv_obj_set_style_bg_color(ui_flashlight, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(ui_indicator1, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(ui_indicator2, lv_color_hex(0xBBBBBB), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_bg_color(ui_flashlight, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(ui_indicator1, lv_color_hex(0xBBBBBB), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(ui_indicator2, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
            }

            flashlightNight = !flashlightNight;
        }

        lv_indev_wait_release(lv_indev_get_act()); // Make sure swipes don't register as clicks
    }, LV_EVENT_GESTURE, NULL);

    lv_obj_add_event_cb(ui_flashlight, [](lv_event_t *e) {
        lv_obj_del(ui_flashlight);
        watch.setBrightness(info.utility.brightness); // reset brightness to proper level
    }, LV_EVENT_SCREEN_UNLOADED, NULL);
}