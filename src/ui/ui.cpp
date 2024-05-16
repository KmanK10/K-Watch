#include "./ui.hpp"

// SCREEN: homeScreen
void initHomeScreen();
lv_obj_t *ui_home;
lv_obj_t *ui_batPercent;
lv_obj_t *ui_timeDigital;
lv_obj_t *ui_date;

void initUI() {
    lv_disp_set_theme(lv_disp_get_default(), lv_theme_default_init(lv_disp_get_default(), lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT));
    initHomeScreen();
    lv_scr_load(ui_home);
}