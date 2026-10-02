#include "pairing.h"

#include "lvgl.h"
#include "ui/screens.h"

#define COLOR_ACCENT lv_color_hex(0xFF5F1F)

static lv_obj_t *s_screen;
static lv_obj_t *s_code;

static void create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_scrollable(s_screen, false);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_label_set_text(title, LV_SYMBOL_BLUETOOTH " Pair iPhone");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    s_code = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_code, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_code, lv_color_white(), 0);
    lv_obj_align(s_code, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *hint = lv_label_create(s_screen);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(hint, "Enter this code\non your phone");
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 56);
}

void pairing_show(uint32_t passkey)
{
    if (!s_screen) {
        create();
    }
    lv_label_set_text_fmt(s_code, "%03lu %03lu", (unsigned long)(passkey / 1000),
                          (unsigned long)(passkey % 1000));
    lv_obj_align(s_code, LV_ALIGN_CENTER, 0, 0);
    ui_show_overlay(s_screen);
}

bool pairing_is_showing(void)
{
    return s_screen && lv_screen_active() == s_screen;
}
