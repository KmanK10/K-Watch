#pragma once

#include <stdint.h>

// Microseconds on the simulator's clock (real time in the window, fake time for screenshots).
int64_t esp_timer_get_time(void);
