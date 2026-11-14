// T-Watch 2020 V3 AXP202. Register map checked against LilyGo's AXP20X driver.
#include "pmu.h"
#include "board_pins.h"

#if defined(BOARD_TWATCH_2020_V3)

#include "esp_check.h"
#include "esp_log.h"
#include "i2c_bus.h"

#define AXP202_ADDR          0x35
#define AXP202_CHIP_ID       0x41
#define REG_STATUS          0x00
#define REG_CHG_STATUS      0x01
#define REG_CHIP_ID         0x03
#define REG_POWER_OUTPUT    0x12
#define REG_LDO24_VOLTAGE   0x28
#define REG_POWER_OFF       0x32
#define REG_CHARGE          0x33
#define REG_INTEN1          0x40
#define REG_INTSTS1         0x48
#define REG_BAT_VOLTAGE     0x78
#define REG_ADC_ENABLE     0x82
#define REG_ADC_CONTROL    0x84
#define REG_BAT_PERCENT    0xB9

#define OUTPUT_DCDC3        (1 << 1) // ESP32 supply: must stay on
#define OUTPUT_LDO2         (1 << 2) // backlight only on V3
#define IRQ1_USB_IN         (1 << 3)
#define IRQ1_USB_OUT        (1 << 2)
#define IRQ2_BAT_IN         (1 << 7)
#define IRQ2_BAT_OUT        (1 << 6)
#define IRQ2_CHARGE_START   (1 << 3)
#define IRQ2_CHARGE_DONE    (1 << 2)
#define IRQ3_BUTTON_SHORT   (1 << 1)
#define IRQ3_BUTTON_LONG    (1 << 0)

static const char *TAG = "pmu";
static i2c_master_dev_handle_t s_dev;

esp_err_t pmu_init(void)
{
    s_dev = i2c_bus_add_main(AXP202_ADDR);
    uint8_t id = 0;
    ESP_RETURN_ON_ERROR(i2c_reg_read(s_dev, REG_CHIP_ID, &id, 1), TAG, "AXP202 not responding");
    if (id != AXP202_CHIP_ID) {
        ESP_LOGE(TAG, "Expected AXP202, got chip id 0x%02X", id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Keep DC3 and its voltage untouched. DC2, LDO3, EXTEN are unused; LDO4
    // supplies the unused audio amp. Backlight starts off until display_wake().
    ESP_RETURN_ON_ERROR(i2c_reg_update(s_dev, REG_POWER_OUTPUT, 0x5F, OUTPUT_DCDC3), TAG, "rails");
    ESP_RETURN_ON_ERROR(i2c_reg_update(s_dev, REG_LDO24_VOLTAGE, 0xF0, 0xF0), TAG, "LDO2 3.3V");

    // Standard 4.2V LiPo, 300mA (AXP202 minimum), termination at 10%.
    ESP_RETURN_ON_ERROR(i2c_reg_write(s_dev, REG_CHARGE, 0xC0), TAG, "charge settings");
    // TS has no thermistor: select ADC input, disable its current source and ADC.
    ESP_RETURN_ON_ERROR(i2c_reg_update(s_dev, REG_ADC_CONTROL, 0x07, 0x04), TAG, "TS settings");
    ESP_RETURN_ON_ERROR(i2c_reg_update(s_dev, REG_ADC_ENABLE, 0xCD, 0xCC), TAG, "battery/VBUS ADC");

    const uint8_t irq_enable[] = {
        IRQ1_USB_IN | IRQ1_USB_OUT,
        IRQ2_BAT_IN | IRQ2_BAT_OUT | IRQ2_CHARGE_START | IRQ2_CHARGE_DONE,
        IRQ3_BUTTON_SHORT | IRQ3_BUTTON_LONG,
        0, 0,
    };
    for (unsigned i = 0; i < sizeof(irq_enable); i++) {
        ESP_RETURN_ON_ERROR(i2c_reg_write(s_dev, REG_INTEN1 + i, irq_enable[i]), TAG, "IRQ enable");
        ESP_RETURN_ON_ERROR(i2c_reg_write(s_dev, REG_INTSTS1 + i, 0xFF), TAG, "IRQ clear");
    }
    ESP_LOGI(TAG, "AXP202 ready, charge limit 4.2V / 300mA");
    return ESP_OK;
}

uint32_t pmu_read_events(void)
{
    uint8_t sts[5] = {0};
    if (i2c_reg_read(s_dev, REG_INTSTS1, sts, sizeof(sts)) != ESP_OK) {
        return 0;
    }
    for (unsigned i = 0; i < sizeof(sts); i++) {
        i2c_reg_write(s_dev, REG_INTSTS1 + i, sts[i]);
    }
    uint32_t evt = 0;
    if (sts[0] & IRQ1_USB_IN)       evt |= PMU_EVT_USB_IN;
    if (sts[0] & IRQ1_USB_OUT)      evt |= PMU_EVT_USB_OUT;
    if (sts[1] & IRQ2_BAT_IN)       evt |= PMU_EVT_BATTERY_IN;
    if (sts[1] & IRQ2_BAT_OUT)      evt |= PMU_EVT_BATTERY_OUT;
    if (sts[1] & IRQ2_CHARGE_START) evt |= PMU_EVT_CHARGE_START;
    if (sts[1] & IRQ2_CHARGE_DONE)  evt |= PMU_EVT_CHARGE_DONE;
    if (sts[2] & IRQ3_BUTTON_SHORT) evt |= PMU_EVT_BUTTON_SHORT;
    if (sts[2] & IRQ3_BUTTON_LONG)  evt |= PMU_EVT_BUTTON_LONG;
    return evt;
}

void pmu_get_status(pmu_status_t *s)
{
    *s = (pmu_status_t){.battery_percent = -1};
    uint8_t status[2] = {0};
    if (i2c_reg_read(s_dev, REG_STATUS, status, sizeof(status)) != ESP_OK) {
        return;
    }
    s->usb_connected = (status[0] & (1 << 5)) != 0;
    s->battery_present = (status[1] & (1 << 5)) != 0;
    s->charging = s->battery_present && (status[1] & (1 << 6));
    if (!s->battery_present) {
        return;
    }
    uint8_t v[2] = {0};
    if (i2c_reg_read(s_dev, REG_BAT_VOLTAGE, v, sizeof(v)) == ESP_OK) {
        // 12-bit ADC, 1.1mV/LSB.
        s->battery_mv = (((uint16_t)v[0] << 4) | (v[1] & 0x0F)) * 11 / 10;
    }
    uint8_t percent = 0;
    if (i2c_reg_read(s_dev, REG_BAT_PERCENT, &percent, 1) == ESP_OK) {
        // Bit 7 means the fuel-gauge value is invalid; never expose it as >100%.
        s->battery_percent = (percent & 0x80) ? 0 : (percent > 100 ? 100 : percent);
    }
}

void pmu_set_backlight_power(bool on)
{
    i2c_reg_update(s_dev, REG_POWER_OUTPUT, OUTPUT_LDO2, on ? OUTPUT_LDO2 : 0);
}

void pmu_set_haptics_power(bool on)
{
    (void)on; // V3 motor is controlled directly by GPIO4.
}

void pmu_power_off(void)
{
    i2c_reg_update(s_dev, REG_POWER_OFF, 0x80, 0x80);
}

#endif
