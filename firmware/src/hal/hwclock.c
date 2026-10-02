// PCF8563 real-time clock.

#include "hwclock.h"

#include "i2c_bus.h"

#define PCF8563_ADDR     0x51
#define REG_CTRL1        0x00
#define REG_SECONDS      0x02
#define REG_CLKOUT       0x0D

#define SECONDS_VL       0x80  // voltage-low flag: time is invalid

static i2c_master_dev_handle_t s_dev;

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(int v)     { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

esp_err_t hwclock_init(void)
{
    s_dev = i2c_bus_add_main(PCF8563_ADDR);
    esp_err_t err = i2c_reg_write(s_dev, REG_CTRL1, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    // CLKOUT is unused and drains the backup supply.
    return i2c_reg_write(s_dev, REG_CLKOUT, 0x00);
}

bool hwclock_get_time(struct tm *t)
{
    uint8_t r[7];
    if (i2c_reg_read(s_dev, REG_SECONDS, r, sizeof(r)) != ESP_OK) {
        return false;
    }
    *t = (struct tm){
        .tm_sec  = bcd2bin(r[0] & 0x7F),
        .tm_min  = bcd2bin(r[1] & 0x7F),
        .tm_hour = bcd2bin(r[2] & 0x3F),
        .tm_mday = bcd2bin(r[3] & 0x3F),
        .tm_wday = r[4] & 0x07,
        .tm_mon  = bcd2bin(r[5] & 0x1F) - 1,
        .tm_year = bcd2bin(r[6]) + 100,
        .tm_isdst = -1,
    };
    return !(r[0] & SECONDS_VL);
}

esp_err_t hwclock_set_time(const struct tm *t)
{
    uint8_t buf[8] = {
        REG_SECONDS,
        bin2bcd(t->tm_sec),
        bin2bcd(t->tm_min),
        bin2bcd(t->tm_hour),
        bin2bcd(t->tm_mday),
        (uint8_t)t->tm_wday,
        bin2bcd(t->tm_mon + 1),
        bin2bcd(t->tm_year % 100),
    };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 50);
}
