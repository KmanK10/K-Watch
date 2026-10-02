#include "theme.h"

lv_obj_t *ui_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);
    return scr;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, "");
    return label;
}

static void on_swipe(lv_event_t *e)
{
    void (*fn)(void) = (void (*)(void))lv_event_get_user_data(e);
    lv_indev_wait_release(lv_indev_active());
    fn();
}

void ui_on_swipe(lv_obj_t *screen, lv_event_code_t gesture, void (*fn)(void))
{
    lv_obj_add_event_cb(screen, on_swipe, gesture, (void *)fn);
}

lv_obj_t *ui_round_button(lv_obj_t *parent, const char *text, int32_t size, lv_color_t bg)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *label = ui_label(btn, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}
