#include "./ui.hpp"

// SCREEN: homeScreen
void initHomeScreen();
lv_obj_t *home;

void initUI() {
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, theme);
    initHomeScreen();
    lv_disp_load_scr(home);
}