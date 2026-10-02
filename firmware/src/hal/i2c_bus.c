#include "i2c_bus.h"

#include "board_pins.h"

#define I2C_SPEED_HZ   400000
#define I2C_TIMEOUT_MS 50

static i2c_master_bus_handle_t s_main_bus;
static i2c_master_bus_handle_t s_touch_bus;

static esp_err_t new_bus(i2c_port_num_t port, int sda, int scl, i2c_master_bus_handle_t *out)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, out);
}

esp_err_t i2c_bus_init(void)
{
    esp_err_t err = new_bus(I2C_NUM_0, BOARD_I2C_SDA, BOARD_I2C_SCL, &s_main_bus);
    if (err != ESP_OK) {
        return err;
    }
    return new_bus(I2C_NUM_1, BOARD_TOUCH_SDA, BOARD_TOUCH_SCL, &s_touch_bus);
}

static i2c_master_dev_handle_t add_device(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_SPEED_HZ,
    };
    i2c_master_dev_handle_t dev = NULL;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &cfg, &dev));
    return dev;
}

i2c_master_dev_handle_t i2c_bus_add_main(uint8_t addr)
{
    return add_device(s_main_bus, addr);
}

i2c_master_dev_handle_t i2c_bus_add_touch(uint8_t addr)
{
    return add_device(s_touch_bus, addr);
}

esp_err_t i2c_reg_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, I2C_TIMEOUT_MS);
}

esp_err_t i2c_reg_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

esp_err_t i2c_reg_update(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t cur;
    esp_err_t err = i2c_reg_read(dev, reg, &cur, 1);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_reg_write(dev, reg, (cur & ~mask) | (value & mask));
}
