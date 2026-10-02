#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef enum {
    IMU_EVT_WRIST_TILT = 1 << 0,
} imu_event_t;

// Loads the BMA423 feature engine and enables step counting and wrist tilt.
// The step counter runs inside the sensor, so the ESP32 can stay asleep.
esp_err_t imu_init(void);

// Reads and clears the interrupt flags. Returns a mask of imu_event_t.
uint32_t imu_read_events(void);

uint32_t imu_get_steps(void);
