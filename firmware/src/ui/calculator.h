#pragma once

// A calculator: a 4x4 keypad under a display that shows the expression so far and the number
// being typed (or a live preview of the answer). × and ÷ are worked out before + and −.
// The button in the display's corner deletes a digit; hold it to clear everything.

#include "lvgl.h"

void calculator_create(lv_obj_t *parent);
