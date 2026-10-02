#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    PMU_EVT_BUTTON_SHORT  = 1 << 0,
    PMU_EVT_BUTTON_LONG   = 1 << 1,
    PMU_EVT_USB_IN        = 1 << 2,
    PMU_EVT_USB_OUT       = 1 << 3,
    PMU_EVT_BATTERY_IN    = 1 << 4,
    PMU_EVT_BATTERY_OUT   = 1 << 5,
    PMU_EVT_CHARGE_START  = 1 << 6,
    PMU_EVT_CHARGE_DONE   = 1 << 7,
} pmu_event_t;

typedef struct {
    bool usb_connected;
    bool battery_present;
    bool charging;
    int battery_percent;   // -1 when no battery
    uint16_t battery_mv;   // 0 when no battery
} pmu_status_t;

esp_err_t pmu_init(void);

// Reads and clears the PMU interrupt flags. Returns a mask of pmu_event_t.
uint32_t pmu_read_events(void);

void pmu_get_status(pmu_status_t *status);

// The backlight has its own supply rail; cutting it saves more than PWM at 0%.
void pmu_set_backlight_power(bool on);

// DRV2605 vibration driver supply. It loses its settings when switched off.
void pmu_set_haptics_power(bool on);
