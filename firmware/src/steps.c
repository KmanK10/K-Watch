#include "steps.h"

#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "imu.h"
#include "nvs.h"

// Saving is rare enough that flash wear doesn't matter, and a crash loses at most this much.
#define SAVE_INTERVAL_US  (5LL * 60 * 1000 * 1000)

static const char *TAG = "steps";

static int32_t s_day;           // year * 1000 + day of year
static uint32_t s_base;         // steps counted today before this boot
static uint32_t s_raw_start;    // sensor count when today began (or at boot)
static uint32_t s_saved;
static int64_t s_saved_at_us;

static int32_t today_key(void)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    return (t.tm_year + 1900) * 1000 + t.tm_yday;
}

void steps_init(void)
{
    s_day = today_key();
    s_raw_start = imu_get_steps();

    nvs_handle_t h;
    if (nvs_open("steps", NVS_READONLY, &h) == ESP_OK) {
        int32_t day = 0;
        uint32_t count = 0;
        if (nvs_get_i32(h, "day", &day) == ESP_OK && nvs_get_u32(h, "count", &count) == ESP_OK &&
            day == s_day) {
            s_base = count;
        }
        nvs_close(h);
    }
    s_saved = s_base;
    s_saved_at_us = esp_timer_get_time();
    ESP_LOGI(TAG, "%lu steps so far today", (unsigned long)s_base);
}

uint32_t steps_today(void)
{
    return s_base + (imu_get_steps() - s_raw_start);
}

void steps_save(bool force)
{
    uint32_t count = steps_today();
    if (count == s_saved) {
        return;
    }
    if (!force && esp_timer_get_time() - s_saved_at_us < SAVE_INTERVAL_US) {
        return;
    }

    nvs_handle_t h;
    if (nvs_open("steps", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (nvs_set_i32(h, "day", s_day) == ESP_OK && nvs_set_u32(h, "count", count) == ESP_OK &&
        nvs_commit(h) == ESP_OK) {
        s_saved = count;
        s_saved_at_us = esp_timer_get_time();
    }
    nvs_close(h);
}

void steps_check_day(void)
{
    int32_t day = today_key();
    if (day == s_day) {
        return;
    }
    ESP_LOGI(TAG, "new day, yesterday's steps: %lu", (unsigned long)steps_today());
    s_day = day;
    s_base = 0;
    s_raw_start = imu_get_steps();
    s_saved = UINT32_MAX;
    steps_save(true);
}

TickType_t steps_ticks_until_midnight(void)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    t.tm_hour = 24;
    t.tm_min = 0;
    t.tm_sec = 1;
    time_t midnight = mktime(&t);
    return pdMS_TO_TICKS((uint32_t)(midnight - now) * 1000);
}
