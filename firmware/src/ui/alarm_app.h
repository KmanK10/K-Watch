#pragma once

// The Alarms app: a list of alarms with on/off switches. Tap one to edit it, or
// add a new one. The editor opens over the list; swipe right there to cancel.

#include "lvgl.h"

void alarm_app_create(lv_obj_t *parent);
void alarm_app_on_show(void);
