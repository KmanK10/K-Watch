#include <LilyGoLib.h>
#include <LV_Helper.h>

#include "lvgl.h"
#include "utility/info.hpp"

// SCREENS
// Screen: main
void initMainScreen(void);
extern lv_obj_t *ui_main;
extern lv_obj_t *ui_mainTV;

// Screen: homeScreen
void initHomeScreen(void);
extern lv_obj_t *ui_home;
extern lv_obj_t *ui_batBar;
extern lv_obj_t *ui_batPercent;
extern lv_obj_t *ui_timeDigital;
extern lv_obj_t *ui_date;

// Screen: apps
void initAppScreen(void);
extern lv_obj_t *ui_apps;

// Screen: settings
void initSettingsScreen(void);
extern lv_obj_t *ui_settings;

// screen: flashlight
void initFlashlight(void);
extern lv_obj_t *ui_flashlight;

void initUI(void);