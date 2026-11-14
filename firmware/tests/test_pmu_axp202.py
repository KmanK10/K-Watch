"""Run the real AXP202 driver against a register model, without ESP32 hardware.

Usage: python3 -m unittest discover -s tests
Requires a host C compiler (CC, or cc).
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

STUBS = {
    "esp_err.h": """
        #pragma once
        typedef int esp_err_t;
        #define ESP_OK 0
        #define ESP_FAIL -1
        #define ESP_ERR_NOT_SUPPORTED 0x106
    """,
    "esp_log.h": """
        #define ESP_LOGE(tag, ...) ((void)(tag))
        #define ESP_LOGI(tag, ...) ((void)(tag))
    """,
    "esp_check.h": """
        #define ESP_RETURN_ON_ERROR(call, tag, ...) do { \
            int err = (call); (void)(tag); if (err != ESP_OK) return err; \
        } while (0)
    """,
    "driver/i2c_master.h": """
        #pragma once
        typedef void *i2c_master_dev_handle_t;
    """,
}

HARNESS = r"""
#include <assert.h>
#include <string.h>
#include "pmu.h"
#include "i2c_bus.h"

static uint8_t regs[256];
static unsigned writes;
static int fail_read = -1, fail_write = -1;

i2c_master_dev_handle_t i2c_bus_add_main(uint8_t addr)
{
    assert(addr == 0x35);
    return regs;
}
esp_err_t i2c_reg_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t len)
{
    assert(dev == regs);
    if (reg == fail_read) return ESP_FAIL;
    memcpy(buf, regs + reg, len);
    return ESP_OK;
}
esp_err_t i2c_reg_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    assert(dev == regs);
    if (reg == fail_write) return ESP_FAIL;
    writes++;
    if (reg >= 0x48 && reg <= 0x4C) regs[reg] &= ~val; // W1C interrupt flags
    else regs[reg] = val;
    return ESP_OK;
}
esp_err_t i2c_reg_update(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t cur;
    esp_err_t err = i2c_reg_read(dev, reg, &cur, 1);
    return err == ESP_OK ? i2c_reg_write(dev, reg, (cur & ~mask) | (val & mask)) : err;
}

int main(void)
{
    // Refuse the wrong PMU before touching its power/charging registers.
    regs[3] = 0x4A;
    assert(pmu_init() == ESP_ERR_NOT_SUPPORTED && writes == 0);
    regs[3] = 0x41;
    fail_read = 3;
    assert(pmu_init() == ESP_FAIL && writes == 0);
    fail_read = -1;
    regs[0x12] = 0xFF;
    regs[0x27] = 0x68; // DC3 voltage: never change the CPU supply voltage
    regs[0x28] = 0x03;
    memset(regs + 0x48, 0xFF, 5);
    assert(pmu_init() == ESP_OK);
    assert((regs[0x12] & 0x5F) == 0x02);
    assert(regs[0x27] == 0x68 && regs[0x28] == 0xF3);
    assert(regs[0x33] == 0xC0); // enabled, 4.2V, 300mA, 10% termination
    assert((regs[0x82] & 0xCD) == 0xCC && (regs[0x84] & 7) == 4);
    assert(regs[0x40] == 0x0C && regs[0x41] == 0xCC && regs[0x42] == 3);
    assert(regs[0x43] == 0 && regs[0x44] == 0);
    for (int i = 0; i < 5; i++) assert(regs[0x48 + i] == 0);
    fail_write = 0x33;
    assert(pmu_init() == ESP_FAIL);
    fail_write = -1;

    const struct { uint8_t reg, bit; uint32_t event; } events[] = {
        {0x48, 3, PMU_EVT_USB_IN}, {0x48, 2, PMU_EVT_USB_OUT},
        {0x49, 7, PMU_EVT_BATTERY_IN}, {0x49, 6, PMU_EVT_BATTERY_OUT},
        {0x49, 3, PMU_EVT_CHARGE_START}, {0x49, 2, PMU_EVT_CHARGE_DONE},
        {0x4A, 1, PMU_EVT_BUTTON_SHORT}, {0x4A, 0, PMU_EVT_BUTTON_LONG},
    };
    for (unsigned i = 0; i < sizeof(events) / sizeof(events[0]); i++) {
        regs[events[i].reg] = 1 << events[i].bit;
        assert(pmu_read_events() == events[i].event);
        assert(pmu_read_events() == 0);
    }
    regs[0x48] = 8;
    fail_read = 0x48;
    assert(pmu_read_events() == 0 && regs[0x48] == 8);
    fail_read = -1;

    pmu_status_t s;
    regs[0] = 0x20;
    regs[1] = 0x60;
    regs[0x78] = 0xD2; regs[0x79] = 0xF8; // ADC 3368 * 1.1 = 3704mV
    regs[0xB9] = 75;
    pmu_get_status(&s);
    assert(s.usb_connected && s.battery_present && s.charging);
    assert(s.battery_mv == 3704 && s.battery_percent == 75);
    regs[0xB9] = 0xFF;
    pmu_get_status(&s);
    assert(s.battery_percent == 0);
    regs[0xB9] = 110;
    pmu_get_status(&s);
    assert(s.battery_percent == 100);
    regs[0] = 0; regs[1] = 0;
    pmu_get_status(&s);
    assert(!s.usb_connected && !s.battery_present && !s.charging);
    assert(s.battery_mv == 0 && s.battery_percent == -1);
    fail_read = 0;
    pmu_get_status(&s);
    assert(!s.battery_present && s.battery_percent == -1);
    fail_read = -1;

    regs[0x12] = 2;
    pmu_set_backlight_power(true);
    assert(regs[0x12] == 6);
    pmu_set_backlight_power(false);
    assert(regs[0x12] == 2); // DC3 stays on
    unsigned before = writes;
    pmu_set_haptics_power(true);
    pmu_set_haptics_power(false);
    assert(writes == before);
    regs[0x32] = 0x46;
    pmu_power_off();
    assert(regs[0x32] == 0xC6);
    return 0;
}
"""


class Axp202Test(unittest.TestCase):
    def test_register_contract(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            for name, content in STUBS.items():
                path = tmp / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
            (tmp / "test.c").write_text(HARNESS)
            exe = tmp / "test"
            subprocess.run([
                os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-DBOARD_TWATCH_2020_V3", f"-I{tmp}", f"-I{ROOT / 'src/hal'}",
                str(ROOT / "src/hal/pmu_axp202.c"), str(tmp / "test.c"), "-o", str(exe),
            ], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()
