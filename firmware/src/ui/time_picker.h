#pragma once

#include <stdint.h>

// Opens hour and minute wheels over the current screen. `done` gets the chosen time, in minutes
// after midnight, when Save is tapped; swiping right closes without saving.
void time_picker_open(const char *title, uint16_t minutes, void (*done)(uint16_t minutes));
