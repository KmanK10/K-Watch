#include "./ui.hpp"

// SCREEN: main
void initMainScreen(void);
lv_obj_t *ui_main;
lv_obj_t *ui_mainTV;

// SCREEN: homeScreen
void initHomeScreen(void);
lv_obj_t *ui_home;
lv_obj_t *ui_batIcon;
lv_obj_t *ui_batBar;
lv_obj_t *ui_batPercent;
lv_obj_t *ui_timeDigital;
lv_obj_t *ui_date;

// SCREEN: apps
void initApps(void);
lv_obj_t *ui_apps;

// SCREEN: flashlight
void initFlashlight(void);
lv_obj_t *ui_flashlight;

void initUI() {
    initMainScreen();

    lv_scr_load(ui_main);
}