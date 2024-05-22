#include <LilyGoLib.h>
#include <LV_Helper.h>

#include "lvgl.h"
#include "info.hpp"

// Screens
void initMainScreen(void);
extern lv_obj_t *ui_main;
extern lv_obj_t *ui_mainTV;

// SCREEN: homeScreen
void initHomeScreen(void);
extern lv_obj_t *ui_home;
extern lv_obj_t *ui_batBar;
extern lv_obj_t *ui_batPercent;
extern lv_obj_t *ui_timeDigital;
extern lv_obj_t *ui_date;

// SCREEN: apps
void initAppScreen(void);
extern lv_obj_t *ui_apps;

// SCREEN: flashlight
void initFlashlight(void);
extern lv_obj_t *ui_flashlight;

void initUI(void);