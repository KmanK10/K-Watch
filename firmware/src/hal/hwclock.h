#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"

esp_err_t hwclock_init(void);

// Returns false if the RTC lost power and the time is not trustworthy.
bool hwclock_get_time(struct tm *t);
esp_err_t hwclock_set_time(const struct tm *t);
