// AXP2101 power management chip. Register map cross-checked against XPowersLib.

#include "pmu.h"

#include "esp_log.h"
#include "i2c_bus.h"

#define AXP2101_ADDR        0x34
#define AXP2101_CHIP_ID     0x4A

#define REG_STATUS1         0x00
#define REG_STATUS2         0x01
#define REG_IC_TYPE         0x03
#define REG_COMMON_CFG      0x10
#define COMMON_SOFT_PWROFF  (1 << 0)
#define REG_VOFF_SET        0x24
#define REG_ADC_CTRL        0x30
#define REG_ADC_VBAT_H      0x34
#define REG_INTEN1          0x40
#define REG_INTSTS1         0x48
#define REG_TS_PIN_CTRL     0x50
#define REG_IPRECHG         0x61
#define REG_ICC_CHG         0x62
#define REG_ITERM_CHG       0x63
#define REG_CV_CHG          0x64
#define REG_BAT_DET_CTRL    0x68
#define REG_DCDC_ONOFF      0x80
#define REG_LDO_ONOFF0      0x90
#define REG_LDO_ONOFF1      0x91
#define REG_ALDO1_VOL       0x92
#define REG_BLDO2_VOL       0x97
#define REG_BAT_PERCENT     0xA4

// REG_LDO_ONOFF0 bits
#define LDO_ALDO1           (1 << 0)  // RTC backup supply
#define LDO_ALDO2           (1 << 1)  // display backlight
#define LDO_ALDO3           (1 << 2)  // touch controller
#define LDO_ALDO4           (1 << 3)  // LoRa radio
#define LDO_BLDO1           (1 << 4)
#define LDO_BLDO2           (1 << 5)  // DRV2605 haptics
#define LDO_CPUSLDO         (1 << 6)
#define LDO_DLDO1           (1 << 7)

// Interrupt bits in INTSTS2 (0x49) and INTSTS3 (0x4A)
#define IRQ2_PKEY_LONG      (1 << 2)
#define IRQ2_PKEY_SHORT     (1 << 3)
#define IRQ2_BAT_REMOVE     (1 << 4)
#define IRQ2_BAT_INSERT     (1 << 5)
#define IRQ2_VBUS_REMOVE    (1 << 6)
#define IRQ2_VBUS_INSERT    (1 << 7)
#define IRQ3_CHG_START      (1 << 3)
#define IRQ3_CHG_DONE       (1 << 4)

#define LDO_MV(mv)          (((mv) - 500) / 100)

// The cells used in this watch are standard 3.7V LiPos: never charge above 4.2V.
#define CV_4V2              3
#define ICC_100MA           4
#define IPRECHG_50MA        2
#define ITERM_25MA          1

static const char *TAG = "pmu";
static i2c_master_dev_handle_t s_dev;

static uint8_t read8(uint8_t reg)
{
    uint8_t v = 0;
    i2c_reg_read(s_dev, reg, &v, 1);
    return v;
}

esp_err_t pmu_init(void)
{
    s_dev = i2c_bus_add_main(AXP2101_ADDR);

    uint8_t id;
    esp_err_t err = i2c_reg_read(s_dev, REG_IC_TYPE, &id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AXP2101 not responding: %s", esp_err_to_name(err));
        return err;
    }
    if (id != AXP2101_CHIP_ID) {
        ESP_LOGW(TAG, "Unexpected chip id 0x%02X", id);
    }

    // Rails: everything at 3.3V, only what is in use switched on.
    for (uint8_t reg = REG_ALDO1_VOL; reg <= REG_BLDO2_VOL; reg++) {
        i2c_reg_update(s_dev, reg, 0x1F, LDO_MV(3300));
    }
    i2c_reg_write(s_dev, REG_LDO_ONOFF0, LDO_ALDO1 | LDO_ALDO3);
    i2c_reg_update(s_dev, REG_LDO_ONOFF1, 0x01, 0);       // DLDO2 off
    i2c_reg_update(s_dev, REG_DCDC_ONOFF, 0x1E, 0);       // DC2-DC5 off, DC1 powers the ESP32

    i2c_reg_update(s_dev, REG_VOFF_SET, 0x07, 0);         // shut down below 2.6V

    // TS pin is not wired to a thermistor; leaving it enabled blocks charging.
    i2c_reg_update(s_dev, REG_TS_PIN_CTRL, 0x1F, 0x10);
    i2c_reg_update(s_dev, REG_ADC_CTRL, 0x0F, 0x0D);      // VBAT, VBUS, VSYS on; TS off
    i2c_reg_update(s_dev, REG_BAT_DET_CTRL, 0x01, 0x01);

    i2c_reg_update(s_dev, REG_IPRECHG, 0x0F, IPRECHG_50MA);
    i2c_reg_update(s_dev, REG_ICC_CHG, 0x1F, ICC_100MA);
    i2c_reg_update(s_dev, REG_ITERM_CHG, 0x0F, ITERM_25MA);
    i2c_reg_update(s_dev, REG_CV_CHG, 0x07, CV_4V2);

    // Interrupts: only the ones we act on.
    i2c_reg_write(s_dev, REG_INTEN1, 0x00);
    i2c_reg_write(s_dev, REG_INTEN1 + 1, IRQ2_PKEY_SHORT | IRQ2_PKEY_LONG |
                                         IRQ2_VBUS_INSERT | IRQ2_VBUS_REMOVE |
                                         IRQ2_BAT_INSERT | IRQ2_BAT_REMOVE);
    i2c_reg_write(s_dev, REG_INTEN1 + 2, IRQ3_CHG_START | IRQ3_CHG_DONE);
    pmu_read_events();

    ESP_LOGI(TAG, "AXP2101 ready, charge limit 4.2V");
    return ESP_OK;
}

uint32_t pmu_read_events(void)
{
    uint8_t sts[3] = {0};
    i2c_reg_read(s_dev, REG_INTSTS1, sts, sizeof(sts));
    for (int i = 0; i < 3; i++) {
        i2c_reg_write(s_dev, REG_INTSTS1 + i, 0xFF);
    }

    uint32_t evt = 0;
    if (sts[1] & IRQ2_PKEY_SHORT)  evt |= PMU_EVT_BUTTON_SHORT;
    if (sts[1] & IRQ2_PKEY_LONG)   evt |= PMU_EVT_BUTTON_LONG;
    if (sts[1] & IRQ2_VBUS_INSERT) evt |= PMU_EVT_USB_IN;
    if (sts[1] & IRQ2_VBUS_REMOVE) evt |= PMU_EVT_USB_OUT;
    if (sts[1] & IRQ2_BAT_INSERT)  evt |= PMU_EVT_BATTERY_IN;
    if (sts[1] & IRQ2_BAT_REMOVE)  evt |= PMU_EVT_BATTERY_OUT;
    if (sts[2] & IRQ3_CHG_START)   evt |= PMU_EVT_CHARGE_START;
    if (sts[2] & IRQ3_CHG_DONE)    evt |= PMU_EVT_CHARGE_DONE;
    return evt;
}

void pmu_get_status(pmu_status_t *s)
{
    uint8_t st1 = read8(REG_STATUS1);
    uint8_t st2 = read8(REG_STATUS2);

    s->usb_connected = (st1 & (1 << 5)) && !(st2 & (1 << 3));
    s->battery_present = st1 & (1 << 3);
    s->charging = (st2 >> 5) == 0x01;

    if (s->battery_present) {
        uint8_t v[2];
        i2c_reg_read(s_dev, REG_ADC_VBAT_H, v, 2);
        s->battery_mv = ((v[0] & 0x1F) << 8) | v[1];
        s->battery_percent = read8(REG_BAT_PERCENT);
    } else {
        s->battery_mv = 0;
        s->battery_percent = -1;
    }
}

void pmu_set_backlight_power(bool on)
{
    i2c_reg_update(s_dev, REG_LDO_ONOFF0, LDO_ALDO2, on ? LDO_ALDO2 : 0);
}

void pmu_set_haptics_power(bool on)
{
    i2c_reg_update(s_dev, REG_LDO_ONOFF0, LDO_BLDO2, on ? LDO_BLDO2 : 0);
}

void pmu_power_off(void)
{
    i2c_reg_update(s_dev, REG_COMMON_CFG, COMMON_SOFT_PWROFF, COMMON_SOFT_PWROFF);
}
