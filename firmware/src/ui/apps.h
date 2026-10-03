#pragma once

// A grid of app icons, for features that don't have their own swipe position.
// An app opens as its own screen over the grid; swipe right to go back.
// To add an app, write its create function and add one line to the table in apps.c.

#include "lvgl.h"

void apps_create(lv_obj_t *parent);
void apps_on_show(void);

// Opens the app with this name, as if its icon was tapped. False if there isn't one.
bool apps_open(const char *name);
