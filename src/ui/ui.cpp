#include "./ui.hpp"

// SSCREENS
// Screen: main
void initMainScreen(void);
lv_obj_t *ui_main;
lv_obj_t *ui_mainTV;

// Screen: homeScreen
void initHomeScreen(void);
lv_obj_t *ui_home;
lv_obj_t *ui_batBar;
lv_obj_t *ui_batPercent;
lv_obj_t *ui_timeDigital;
lv_obj_t *ui_date;

// Screen: apps
void initApps(void);
lv_obj_t *ui_apps;

// Screen: settings
void initSettingsScreen(void);
lv_obj_t *ui_settings;

// Screen: flashlight
void initFlashlight(void);
lv_obj_t *ui_flashlight;

void initUI() {
    initMainScreen();

    lv_scr_load(ui_main);
}