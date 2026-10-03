// DRV2605 haptic driver with an ERM motor, on a PMU rail that is off when idle.

#include "haptics.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "pmu.h"

#define DRV2605_ADDR     0x5A

#define REG_STATUS       0x00
#define REG_MODE         0x01
#define REG_LIBRARY      0x03
#define REG_WAVESEQ1     0x04
#define REG_GO           0x0C

#define MODE_INTTRIG     0x00
#define LIBRARY_ERM_A    0x01
#define WAIT(ms)         (0x80 | ((ms) / 10))

// Effect numbers from the DRV2605 ROM library (datasheet table 11.2).
#define FX_STRONG_CLICK  1
#define FX_SHARP_CLICK   4
#define FX_STRONG_BUZZ   14
#define FX_BUZZ          47

typedef struct {
    uint8_t seq[8];
    uint32_t duration_ms;
} pattern_t;

static const pattern_t s_patterns[] = {
    [HAPTIC_TAP]    = {{FX_STRONG_CLICK, 0}, 300},
    [HAPTIC_TICK]   = {{FX_SHARP_CLICK, 0}, 300},
    [HAPTIC_NOTIFY] = {{FX_BUZZ, WAIT(150), FX_BUZZ, 0}, 800},
    [HAPTIC_ALERT]  = {{FX_STRONG_BUZZ, WAIT(300), FX_STRONG_BUZZ, WAIT(300), FX_STRONG_BUZZ, 0}, 2000},
};

static const char *TAG = "haptics";
static i2c_master_dev_handle_t s_dev;
static esp_timer_handle_t s_off_timer;

static volatile bool s_powered;
static bool s_touch_feedback = true;

static void power_off_cb(void *arg)
{
    (void)arg;
    s_powered = false;
    pmu_set_haptics_power(false);
}

static esp_err_t power_up(void)
{
    // Back-to-back clicks (a scroll wheel) find it still on and skip the wait.
    if (s_powered) {
        return ESP_OK;
    }
    pmu_set_haptics_power(true);
    // The chip needs about 250us after power-up before it answers.
    vTaskDelay(pdMS_TO_TICKS(2));
    esp_err_t err = i2c_reg_write(s_dev, REG_MODE, MODE_INTTRIG);
    s_powered = err == ESP_OK;
    return err;
}

esp_err_t haptics_init(void)
{
    s_dev = i2c_bus_add_main(DRV2605_ADDR);
    const esp_timer_create_args_t args = {.callback = power_off_cb, .name = "haptics_off"};
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_off_timer));

    esp_err_t err = power_up();
    uint8_t status = 0;
    if (err == ESP_OK) {
        err = i2c_reg_read(s_dev, REG_STATUS, &status, 1);
    }
    s_powered = false;
    pmu_set_haptics_power(false);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DRV2605 not responding");
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
        return err;
    }
    ESP_LOGI(TAG, "DRV2605 ready (id %d)", status >> 5);
    return ESP_OK;
}

void haptics_play(haptic_pattern_t pattern)
{
    if (!s_dev || pattern >= sizeof(s_patterns) / sizeof(s_patterns[0])) {
        return;
    }
    if (!s_touch_feedback && (pattern == HAPTIC_TAP || pattern == HAPTIC_TICK)) {
        return;
    }
    const pattern_t *p = &s_patterns[pattern];

    esp_timer_stop(s_off_timer);
    if (power_up() != ESP_OK) {
        s_powered = false;
        pmu_set_haptics_power(false);
        return;
    }
    i2c_reg_write(s_dev, REG_LIBRARY, LIBRARY_ERM_A);
    for (int i = 0; i < 8; i++) {
        i2c_reg_write(s_dev, REG_WAVESEQ1 + i, p->seq[i]);
        if (p->seq[i] == 0) {
            break;
        }
    }
    i2c_reg_write(s_dev, REG_GO, 1);
    esp_timer_start_once(s_off_timer, (uint64_t)p->duration_ms * 1000);
}

void haptics_set_touch_feedback(bool on)
{
    s_touch_feedback = on;
}
