// BMA423 accelerometer. Init sequence follows the Bosch BMA4xx driver.

#include "imu.h"

#include <string.h>

#include "bma423_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#define BMA423_ADDR_PRIMARY   0x18
#define BMA423_ADDR_SECONDARY 0x19
#define BMA423_CHIP_ID        0x13

#define REG_CHIP_ID           0x00
#define REG_INT_STATUS_0      0x1C
#define REG_STEP_COUNTER      0x1E
#define REG_INTERNAL_STATUS   0x2A
#define REG_ACC_CONF          0x40
#define REG_ACC_RANGE         0x41
#define REG_INT1_IO_CTRL      0x53
#define REG_INT_LATCH         0x55
#define REG_INT1_MAP          0x56
#define REG_INIT_CTRL         0x59
#define REG_ASIC_LSB          0x5B
#define REG_ASIC_MSB          0x5C
#define REG_FEATURES          0x5E
#define REG_PWR_CONF          0x7C
#define REG_PWR_CTRL          0x7D
#define REG_CMD               0x7E

#define CMD_SOFT_RESET        0xB6
#define ASIC_INITIALIZED      0x01

#define FEATURE_SIZE          64
#define FEAT_STEP_CNTR        0x36
#define FEAT_TILT             0x3A
#define FEAT_AXES_REMAP       0x3E
#define STEP_CNTR_EN          0x10
#define TILT_EN               0x01

#define INT_TILT              0x08

// The sensor sits on the bottom of the board, top-right corner: x<-y, y<-x, z<- -z.
#define REMAP_BYTE0           0x81
#define REMAP_BYTE1           0x01

#define CONFIG_CHUNK          32

static const char *TAG = "imu";
static i2c_master_dev_handle_t s_dev;
static uint8_t s_feature_lsb;
static uint8_t s_feature_msb;

static esp_err_t write_block(uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t buf[1 + FEATURE_SIZE];
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    return i2c_master_transmit(s_dev, buf, len + 1, 100);
}

static esp_err_t load_config(void)
{
    i2c_reg_write(s_dev, REG_PWR_CONF, 0x00);
    vTaskDelay(pdMS_TO_TICKS(1));
    i2c_reg_write(s_dev, REG_INIT_CTRL, 0x00);

    for (uint16_t i = 0; i < BMA423_CONFIG_SIZE; i += CONFIG_CHUNK) {
        i2c_reg_write(s_dev, REG_ASIC_LSB, (i / 2) & 0x0F);
        i2c_reg_write(s_dev, REG_ASIC_MSB, (i / 2) >> 4);
        esp_err_t err = write_block(REG_FEATURES, &bma423_config_file[i], CONFIG_CHUNK);
        if (err != ESP_OK) {
            return err;
        }
    }

    i2c_reg_write(s_dev, REG_INIT_CTRL, 0x01);
    vTaskDelay(pdMS_TO_TICKS(150));

    uint8_t status = 0;
    i2c_reg_read(s_dev, REG_INTERNAL_STATUS, &status, 1);
    if ((status & 0x0F) != ASIC_INITIALIZED) {
        ESP_LOGE(TAG, "Feature engine failed to start (status 0x%02X)", status);
        return ESP_FAIL;
    }

    i2c_reg_read(s_dev, REG_ASIC_LSB, &s_feature_lsb, 1);
    i2c_reg_read(s_dev, REG_ASIC_MSB, &s_feature_msb, 1);
    return ESP_OK;
}

static void select_feature_page(void)
{
    i2c_reg_write(s_dev, REG_ASIC_LSB, s_feature_lsb & 0x0F);
    i2c_reg_write(s_dev, REG_ASIC_MSB, s_feature_msb);
}

static esp_err_t configure_features(void)
{
    uint8_t f[FEATURE_SIZE];
    select_feature_page();
    esp_err_t err = i2c_reg_read(s_dev, REG_FEATURES, f, sizeof(f));
    if (err != ESP_OK) {
        return err;
    }

    f[FEAT_AXES_REMAP] = REMAP_BYTE0;
    f[FEAT_AXES_REMAP + 1] = REMAP_BYTE1;
    f[FEAT_STEP_CNTR + 1] |= STEP_CNTR_EN;
    f[FEAT_TILT] |= TILT_EN;

    select_feature_page();
    return write_block(REG_FEATURES, f, sizeof(f));
}

static bool probe(uint8_t addr)
{
    s_dev = i2c_bus_add_main(addr);
    uint8_t id = 0;
    if (i2c_reg_read(s_dev, REG_CHIP_ID, &id, 1) == ESP_OK && id == BMA423_CHIP_ID) {
        return true;
    }
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    return false;
}

esp_err_t imu_init(void)
{
    if (!probe(BMA423_ADDR_PRIMARY) && !probe(BMA423_ADDR_SECONDARY)) {
        ESP_LOGE(TAG, "BMA423 not found");
        return ESP_ERR_NOT_FOUND;
    }

    // The chip does not acknowledge the reset command, so ignore the result.
    i2c_reg_write(s_dev, REG_CMD, CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));

    esp_err_t err = load_config();
    if (err != ESP_OK) {
        return err;
    }

    i2c_reg_write(s_dev, REG_PWR_CTRL, 0x04);       // accelerometer on
    i2c_reg_write(s_dev, REG_ACC_CONF, 0x28);       // 100Hz, averaging, low-power mode
    i2c_reg_write(s_dev, REG_ACC_RANGE, 0x00);      // +-2g

    err = configure_features();
    if (err != ESP_OK) {
        return err;
    }

    // INT1: push-pull, active high, latched until INT_STATUS_0 is read.
    i2c_reg_write(s_dev, REG_INT1_IO_CTRL, 0x0A);
    i2c_reg_write(s_dev, REG_INT_LATCH, 0x01);
    i2c_reg_write(s_dev, REG_INT1_MAP, INT_TILT);

    i2c_reg_write(s_dev, REG_PWR_CONF, 0x03);       // advanced power save
    imu_read_events();

    ESP_LOGI(TAG, "BMA423 ready: steps + wrist tilt");
    return ESP_OK;
}

uint32_t imu_read_events(void)
{
    if (!s_dev) {
        return 0;
    }
    uint8_t sts = 0;
    i2c_reg_read(s_dev, REG_INT_STATUS_0, &sts, 1);
    return (sts & INT_TILT) ? IMU_EVT_WRIST_TILT : 0;
}

uint32_t imu_get_steps(void)
{
    if (!s_dev) {
        return 0;
    }
    uint8_t d[4] = {0};
    i2c_reg_read(s_dev, REG_STEP_COUNTER, d, sizeof(d));
    return d[0] | (d[1] << 8) | (d[2] << 16) | ((uint32_t)d[3] << 24);
}
