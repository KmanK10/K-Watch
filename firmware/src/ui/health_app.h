#pragma once

// The Health app: today's steps against the daily goal, 7 and 30 day averages,
// a chart of the last week, and the goal setting.

#include "lvgl.h"

void health_app_create(lv_obj_t *parent);
void health_app_on_show(void);
bool health_app_is_showing(void);

// A full-screen "Goal reached!" that closes itself after a few seconds, or on a tap.
void health_show_goal_reached(void);
