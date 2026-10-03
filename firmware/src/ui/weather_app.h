#pragma once

// The Weather app: current conditions, the next hours, details and the week ahead, from the
// forecast the companion app last sent. The refresh button asks the phone for a new one.

#include "lvgl.h"

// The page is filled in by weather_app_on_show, which the apps page calls before every opening.
void weather_app_create(lv_obj_t *parent);
void weather_app_on_show(void);

// Redraws if the app is open, e.g. after new weather arrived.
void weather_app_refresh(void);

// Called when refresh is tapped.
void weather_app_on_refresh(void (*cb)(void));
