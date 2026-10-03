#include "steps.h"

#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "imu.h"
#include "nvs.h"

// Saving is rare enough that flash wear doesn't matter, and a crash loses at most this much.
#define SAVE_INTERVAL_US  (5LL * 60 * 1000 * 1000)

#define HISTORY_VERSION   1
#define NO_DATA           UINT32_MAX
// Before this the clock hasn't been set yet, so the date can't be trusted.
#define FIRST_VALID_YEAR  2024

#define DEFAULT_GOAL      8000

static const char *TAG = "steps";

static int32_t s_day;           // year * 1000 + day of year
static uint32_t s_base;         // steps counted today before this boot
static uint32_t s_raw_start;    // sensor count when today began (or at boot)
static uint32_t s_saved;
static int64_t s_saved_at_us;
static uint32_t s_goal = DEFAULT_GOAL;
static void (*s_on_goal_change)(void);

// Totals of finished days. steps[i] is for the day `i` days before last_day.
typedef struct {
    uint8_t version;
    int32_t last_day;           // day number, 0 while empty
    uint32_t steps[STEPS_HISTORY_DAYS];
} history_t;

static history_t s_history;

static int32_t today_key(void)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    return (t.tm_year + 1900) * 1000 + t.tm_yday;
}

static bool key_valid(int32_t key)
{
    return key / 1000 >= FIRST_VALID_YEAR;
}

// Counts days, so the gap between two dates is a subtraction.
static int32_t day_number(int32_t key)
{
    // Noon, so a daylight saving change can't push it into the next or previous day.
    struct tm t = {
        .tm_year = key / 1000 - 1900,
        .tm_mday = key % 1000 + 1,
        .tm_hour = 12,
        .tm_isdst = -1,
    };
    return (int32_t)(mktime(&t) / 86400);
}

static void history_clear(void)
{
    s_history.version = HISTORY_VERSION;
    s_history.last_day = 0;
    for (int i = 0; i < STEPS_HISTORY_DAYS; i++) {
        s_history.steps[i] = NO_DATA;
    }
}

static void history_save(void)
{
    nvs_handle_t h;
    if (nvs_open("steps", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(h, "history", &s_history, sizeof(s_history)) == ESP_OK) {
        nvs_commit(h);
    }
    nvs_close(h);
}

// Stores a finished day. Days in between that were never recorded are left as no data.
static void history_record(int32_t key, uint32_t count)
{
    if (!key_valid(key)) {
        return;
    }
    int32_t day = day_number(key);
    if (s_history.last_day == 0) {
        s_history.last_day = day;
    }
    int32_t shift = day - s_history.last_day;
    if (shift < 0) {
        // An earlier day, e.g. after the clock was corrected.
        if (-shift < STEPS_HISTORY_DAYS) {
            s_history.steps[-shift] = count;
            history_save();
        }
        return;
    }
    if (shift >= STEPS_HISTORY_DAYS) {
        history_clear();
    } else if (shift > 0) {
        memmove(&s_history.steps[shift], &s_history.steps[0],
                (STEPS_HISTORY_DAYS - shift) * sizeof(s_history.steps[0]));
        for (int i = 1; i < shift; i++) {
            s_history.steps[i] = NO_DATA;
        }
    }
    s_history.last_day = day;
    s_history.steps[0] = count;
    history_save();
    ESP_LOGI(TAG, "recorded %lu steps for day %ld", (unsigned long)count, (long)day);
}

void steps_init(void)
{
    s_day = today_key();
    s_raw_start = imu_get_steps();
    history_clear();

    nvs_handle_t h;
    int32_t day = 0;
    uint32_t count = 0;
    bool have_count = false;
    if (nvs_open("steps", NVS_READONLY, &h) == ESP_OK) {
        have_count = nvs_get_i32(h, "day", &day) == ESP_OK && nvs_get_u32(h, "count", &count) == ESP_OK;
        history_t saved;
        size_t len = sizeof(saved);
        if (nvs_get_blob(h, "history", &saved, &len) == ESP_OK && len == sizeof(saved) &&
            saved.version == HISTORY_VERSION) {
            s_history = saved;
        }
        uint32_t goal;
        if (nvs_get_u32(h, "goal", &goal) == ESP_OK) {
            s_goal = goal;
        }
        nvs_close(h);
    }
    if (have_count && day == s_day) {
        s_base = count;
    } else if (have_count) {
        // The watch was off over midnight, so the last day it counted never got recorded.
        history_record(day, count);
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
    uint32_t yesterday = steps_today();
    ESP_LOGI(TAG, "new day, yesterday's steps: %lu", (unsigned long)yesterday);
    history_record(s_day, yesterday);
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

bool steps_on_day(int days_ago, uint32_t *count)
{
    if (days_ago == 0) {
        *count = steps_today();
        return true;
    }
    if (!key_valid(s_day) || s_history.last_day == 0) {
        return false;
    }
    int32_t i = s_history.last_day - (day_number(s_day) - days_ago);
    if (i < 0 || i >= STEPS_HISTORY_DAYS || s_history.steps[i] == NO_DATA) {
        return false;
    }
    *count = s_history.steps[i];
    return true;
}

uint32_t steps_average(int days, int *days_with_data)
{
    uint64_t sum = 0;
    int n = 0;
    for (int d = 1; d <= days; d++) {
        uint32_t count;
        if (steps_on_day(d, &count)) {
            sum += count;
            n++;
        }
    }
    if (days_with_data) {
        *days_with_data = n;
    }
    return n ? (uint32_t)(sum / n) : 0;
}

uint32_t steps_goal(void)
{
    return s_goal;
}

void steps_set_goal(uint32_t goal)
{
    if (goal == s_goal) {
        return;
    }
    s_goal = goal;
    nvs_handle_t h;
    if (nvs_open("steps", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (nvs_set_u32(h, "goal", goal) == ESP_OK) {
        nvs_commit(h);
    }
    nvs_close(h);
    if (s_on_goal_change) {
        s_on_goal_change();
    }
}

void steps_on_goal_change(void (*cb)(void))
{
    s_on_goal_change = cb;
}
