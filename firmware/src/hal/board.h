#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    BOARD_EVT_PMU,     // button, USB or charger event: call pmu_read_events()
    BOARD_EVT_TOUCH,   // finger down while the screen was off
    BOARD_EVT_IMU,     // motion feature fired: call imu_read_events()
} board_event_t;

// Brings up every peripheral, sets the system clock from the RTC and
// configures automatic light sleep. Interrupts are posted to `events`.
esp_err_t board_init(QueueHandle_t events);

// Interrupt lines are level-triggered and disarm themselves when they fire.
void board_rearm(board_event_t evt);

// Touch only needs to wake the watch while the screen is off.
void board_set_touch_wake(bool enable);

// Light sleep stops the backlight PWM and SPI, so block it while the screen is on.
void board_set_screen_awake(bool awake);
