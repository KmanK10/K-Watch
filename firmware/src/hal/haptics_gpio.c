// T-Watch 2020 V3 motor, driven through a transistor on GPIO4.
#include "haptics.h"
#include "board_pins.h"

#if defined(BOARD_TWATCH_2020_V3)

#include "driver/gpio.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Alternating on/off durations, terminated by zero.
static const uint16_t s_patterns[][6] = {
    [HAPTIC_TAP]    = {40, 0},
    [HAPTIC_TICK]   = {20, 0},
    [HAPTIC_NOTIFY] = {180, 150, 180, 0},
    [HAPTIC_ALERT]  = {400, 300, 400, 300, 400, 0},
};

static QueueHandle_t s_requests;
static esp_pm_lock_handle_t s_motor_lock;
static bool s_touch_feedback = true;

static void motor_task(void *arg)
{
    (void)arg;
    haptic_pattern_t pattern;
    bool pending = false;
    while (true) {
        if (!pending) {
            xQueueReceive(s_requests, &pattern, portMAX_DELAY);
        }
        pending = false;
        esp_pm_lock_acquire(s_motor_lock);
        for (unsigned i = 0; i < 6 && s_patterns[pattern][i]; i++) {
            gpio_set_level(BOARD_MOTOR_PIN, (i % 2) == 0);
            TickType_t wait = pdMS_TO_TICKS(s_patterns[pattern][i]);
            // A new request interrupts the current pattern without blocking the UI.
            haptic_pattern_t next;
            if (xQueueReceive(s_requests, &next, wait ? wait : 1) == pdTRUE) {
                pattern = next;
                pending = true;
                break;
            }
        }
        gpio_set_level(BOARD_MOTOR_PIN, 0);
        esp_pm_lock_release(s_motor_lock);
    }
}

esp_err_t haptics_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_MOTOR_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(BOARD_MOTOR_PIN, 0);
    gpio_sleep_sel_dis(BOARD_MOTOR_PIN);
    esp_err_t err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "motor", &s_motor_lock);
    if (err != ESP_OK) {
        return err;
    }
    s_requests = xQueueCreate(1, sizeof(haptic_pattern_t));
    if (!s_requests) {
        esp_pm_lock_delete(s_motor_lock);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(motor_task, "haptics", 2048, NULL, 2, NULL) != pdPASS) {
        vQueueDelete(s_requests);
        s_requests = NULL;
        esp_pm_lock_delete(s_motor_lock);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void haptics_play(haptic_pattern_t pattern)
{
    if (!s_requests || (unsigned)pattern >= sizeof(s_patterns) / sizeof(s_patterns[0])) {
        return;
    }
    if (!s_touch_feedback && (pattern == HAPTIC_TAP || pattern == HAPTIC_TICK)) {
        return;
    }
    xQueueOverwrite(s_requests, &pattern);
}

void haptics_set_touch_feedback(bool on)
{
    s_touch_feedback = on;
}

#endif
