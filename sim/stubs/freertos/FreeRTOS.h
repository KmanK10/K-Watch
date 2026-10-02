#pragma once

#include <stdint.h>

typedef uint32_t TickType_t;

#define portMAX_DELAY       ((TickType_t)0xffffffffUL)
#define pdMS_TO_TICKS(ms)   ((TickType_t)(ms))
