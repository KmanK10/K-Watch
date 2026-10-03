#include "board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "board_pins.h"
#include "display.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "i2c_bus.h"
#include "pmu.h"
#include "haptics.h"
#include "hwclock.h"
#include "imu.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "touch.h"

static const char *TAG = "board";

static QueueHandle_t s_events;
static esp_pm_lock_handle_t s_screen_lock;
static bool s_imu_ok;
static int32_t s_utc_offset;

static gpio_num_t event_pin(board_event_t evt)
{
    switch (evt) {
    case BOARD_EVT_PMU:
        return BOARD_PMU_INT;
    case BOARD_EVT_TOUCH:
        return BOARD_TOUCH_INT;
    case BOARD_EVT_IMU:
        return BOARD_BMA423_INT1;
    }
    return GPIO_NUM_NC;
}

// The BMA423 drives its line high; the PMU and touch pull theirs low.
static bool event_active_high(board_event_t evt)
{
    return evt == BOARD_EVT_IMU;
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
    bool high = event_active_high(evt);
    gpio_int_type_t level = high ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = high ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE,
        .pull_down_en = high ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = level,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    // Keep the normal pin config during light sleep instead of isolating it.
    gpio_sleep_sel_dis(pin);
    ESP_ERROR_CHECK(gpio_isr_handler_add(pin, irq_handler, (void *)(uintptr_t)evt));
    ESP_ERROR_CHECK(gpio_wakeup_enable(pin, level));
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

static void apply_utc_offset(int32_t seconds)
{
    // POSIX counts the offset the other way: "LOC+7:00" is seven hours behind UTC.
    int32_t a = seconds < 0 ? -seconds : seconds;
    char tz[24];
    snprintf(tz, sizeof(tz), "LOC%c%ld:%02ld", seconds <= 0 ? '+' : '-', (long)(a / 3600),
             (long)(a % 3600 / 60));
    setenv("TZ", tz, 1);
    tzset();
    s_utc_offset = seconds;
}

static void load_utc_offset(void)
{
    nvs_handle_t h;
    int32_t seconds = 0;
    if (nvs_open("clock", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, "utc_offset", &seconds);
        nvs_close(h);
    }
    apply_utc_offset(seconds);
}

void board_set_utc_offset(int32_t seconds)
{
    if (seconds == s_utc_offset || seconds < -14 * 3600 || seconds > 14 * 3600) {
        return;
    }
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tv.tv_sec += s_utc_offset - seconds;
    apply_utc_offset(seconds);
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "UTC offset %+ld min", (long)(seconds / 60));

    nvs_handle_t h;
    if (nvs_open("clock", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, "utc_offset", seconds);
        nvs_commit(h);
        nvs_close(h);
    }
}

int32_t board_utc_offset(void)
{
    return s_utc_offset;
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

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(pmu_init());
    ESP_ERROR_CHECK(hwclock_init());
    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(display_init());
    // Without these the watch still works, just without wrist wake, steps or vibration.
    s_imu_ok = imu_init() == ESP_OK;
    haptics_init();

    load_utc_offset();
    sync_clock_from_rtc();

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    irq_pin_init(BOARD_EVT_PMU);
    irq_pin_init(BOARD_EVT_TOUCH);
    if (s_imu_ok) {
        irq_pin_init(BOARD_EVT_IMU);
    }
    board_set_touch_wake(false);
    keep_display_pins_in_sleep();

    power_management_init();
    return ESP_OK;
}

void board_set_time(const struct tm *t)
{
    struct tm copy = *t;
    hwclock_set_time(&copy);
    struct timeval tv = {.tv_sec = mktime(&copy)};
    settimeofday(&tv, NULL);
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
