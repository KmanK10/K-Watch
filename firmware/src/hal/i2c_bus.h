#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

// The watch has two I2C buses: the main one (PMU, RTC, IMU, haptics) and a
// separate one for the touch controller.
esp_err_t i2c_bus_init(void);
i2c_master_dev_handle_t i2c_bus_add_main(uint8_t addr);
i2c_master_dev_handle_t i2c_bus_add_touch(uint8_t addr);

esp_err_t i2c_reg_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len);
esp_err_t i2c_reg_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value);
esp_err_t i2c_reg_update(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t mask, uint8_t value);
