#include "board.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "board_pins.h"
#include "display.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "i2c_bus.h"
#include "pmu.h"
#include "hwclock.h"
#include "touch.h"

static const char *TAG = "board";

static QueueHandle_t s_events;
static esp_pm_lock_handle_t s_screen_lock;

static gpio_num_t event_pin(board_event_t evt)
{
    return evt == BOARD_EVT_PMU ? BOARD_PMU_INT : BOARD_TOUCH_INT;
}

static void irq_handler(void *arg)
{
    board_event_t evt = (board_event_t)(uintptr_t)arg;
    gpio_intr_disable(event_pin(evt));
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_events, &evt, &woken);
    portYIELD_FROM_ISR(woken);
}

static void irq_pin_init(board_event_t evt)
{
    gpio_num_t pin = event_pin(evt);
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_LOW_LEVEL,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    // Keep the normal pin config during light sleep instead of isolating it.
    gpio_sleep_sel_dis(pin);
    ESP_ERROR_CHECK(gpio_isr_handler_add(pin, irq_handler, (void *)(uintptr_t)evt));
    ESP_ERROR_CHECK(gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL));
}

static void keep_display_pins_in_sleep(void)
{
    // Floating SPI lines could clock garbage into the sleeping panel.
    const gpio_num_t pins[] = {BOARD_TFT_CS, BOARD_TFT_DC, BOARD_TFT_SCLK, BOARD_TFT_MOSI, BOARD_TFT_BL};
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        gpio_sleep_sel_dis(pins[i]);
    }
}

static void set_build_time(struct tm *t)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {0};
    int day = 1, year = 2026;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &t->tm_hour, &t->tm_min, &t->tm_sec);
    t->tm_mon = (int)((strstr(months, mon) - months) / 3);
    t->tm_mday = day;
    t->tm_year = year - 1900;
    t->tm_isdst = -1;
}

static void sync_clock_from_rtc(void)
{
    struct tm build = {0};
    set_build_time(&build);
    time_t build_time = mktime(&build);

    // A time before this firmware was built is certainly wrong. Use the build
    // time until the phone provides the real time.
    struct tm t = {0};
    if (!hwclock_get_time(&t) || mktime(&t) < build_time) {
        t = build;
        hwclock_set_time(&t);
        ESP_LOGW(TAG, "RTC time invalid, set to build time");
    }
    struct timeval tv = {.tv_sec = mktime(&t)};
    settimeofday(&tv, NULL);

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    ESP_LOGI(TAG, "Clock set to %s", buf);
}

static void power_management_init(void)
{
    esp_pm_config_t pm = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm));
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "screen", &s_screen_lock));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
}

esp_err_t board_init(QueueHandle_t events)
{
    s_events = events;

    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(pmu_init());
    ESP_ERROR_CHECK(hwclock_init());
    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(display_init());

    sync_clock_from_rtc();

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    irq_pin_init(BOARD_EVT_PMU);
    irq_pin_init(BOARD_EVT_TOUCH);
    board_set_touch_wake(false);
    keep_display_pins_in_sleep();

    power_management_init();
    return ESP_OK;
}

void board_rearm(board_event_t evt)
{
    gpio_intr_enable(event_pin(evt));
}

void board_set_touch_wake(bool enable)
{
    if (enable) {
        gpio_wakeup_enable(BOARD_TOUCH_INT, GPIO_INTR_LOW_LEVEL);
        gpio_intr_enable(BOARD_TOUCH_INT);
    } else {
        gpio_intr_disable(BOARD_TOUCH_INT);
        gpio_wakeup_disable(BOARD_TOUCH_INT);
    }
}

void board_set_screen_awake(bool awake)
{
    if (awake) {
        esp_pm_lock_acquire(s_screen_lock);
    } else {
        esp_pm_lock_release(s_screen_lock);
    }
}
