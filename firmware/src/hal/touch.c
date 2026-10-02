// FT6336 capacitive touch controller.

#include "touch.h"

#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "i2c_bus.h"

#define FT6336_ADDR      0x38
#define REG_TD_STATUS    0x02
#define REG_CTRL         0x86  // 1 = drop to monitor mode automatically
#define REG_G_MODE       0xA4  // 0 = INT held low while touched
#define REG_POWER_MODE   0xA5
#define REG_CHIP_ID      0xA3

#define POWER_ACTIVE     0x00
#define POWER_MONITOR    0x01

static const char *TAG = "touch";
static i2c_master_dev_handle_t s_dev;

esp_err_t touch_init(void)
{
    s_dev = i2c_bus_add_touch(FT6336_ADDR);

    uint8_t id;
    esp_err_t err = i2c_reg_read(s_dev, REG_CHIP_ID, &id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FT6336 not responding: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "FT6336 chip id 0x%02X", id);

    i2c_reg_write(s_dev, REG_G_MODE, 0x00);
    i2c_reg_write(s_dev, REG_CTRL, 0x01);
    return ESP_OK;
}

bool touch_read(int16_t *x, int16_t *y)
{
    if (gpio_get_level(BOARD_TOUCH_INT) != 0) {
        return false;
    }

    uint8_t d[5];
    if (i2c_reg_read(s_dev, REG_TD_STATUS, d, sizeof(d)) != ESP_OK || (d[0] & 0x0F) == 0) {
        return false;
    }
    *x = ((d[1] & 0x0F) << 8) | d[2];
    *y = ((d[3] & 0x0F) << 8) | d[4];
    return true;
}

void touch_set_low_power(bool low_power)
{
    i2c_reg_write(s_dev, REG_POWER_MODE, low_power ? POWER_MONITOR : POWER_ACTIVE);
}
