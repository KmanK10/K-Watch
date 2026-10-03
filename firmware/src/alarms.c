#include "alarms.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "nvs.h"
#include "settings.h"

#define SAVE_VERSION        1
#define SNOOZE_S            (9 * 60)
#define MAX_AUTO_SNOOZES    3
// If the clock jumps far past an alarm (e.g. the time was just set), skip it instead of ringing late.
#define LATE_LIMIT_S        (10 * 60)
// Before the phone or the clock chip sets the time, the clock starts in 1970.
#define VALID_AFTER         1700000000

static const char *TAG = "alarms";

typedef struct {
    uint8_t version;
    uint8_t count;
    alarm_t alarms[ALARMS_MAX];
} saved_t;

static alarm_t s_alarms[ALARMS_MAX];
static size_t s_count;

static time_t s_next_due;       // 0 when nothing is due
static size_t s_next_which;
static time_t s_snooze_due;     // 0 when nothing is snoozed
static size_t s_ringing = SIZE_MAX;
static int s_auto_snoozes;

static void save(void)
{
    saved_t saved = {.version = SAVE_VERSION, .count = (uint8_t)s_count};
    memcpy(saved.alarms, s_alarms, sizeof(s_alarms));
    nvs_handle_t h;
    if (nvs_open("alarms", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(h, "v", &saved, sizeof(saved)) != ESP_OK || nvs_commit(h) != ESP_OK) {
        ESP_LOGW(TAG, "could not save alarms");
    }
    nvs_close(h);
}

// The first time this alarm rings after `after`, or 0 if it never will.
static time_t next_ring(const alarm_t *a, time_t after)
{
    struct tm day;
    localtime_r(&after, &day);
    for (int offset = 0; offset <= 7; offset++) {
        struct tm t = day;
        t.tm_mday += offset;
        t.tm_hour = a->hour;
        t.tm_min = a->minute;
        t.tm_sec = 0;
        t.tm_isdst = -1;
        time_t when = mktime(&t);   // also fills in tm_wday for the shifted date
        if (when > after && (a->days == 0 || (a->days & (1 << t.tm_wday)))) {
            return when;
        }
    }
    return 0;
}

static void plan(time_t after)
{
    s_next_due = 0;
    if (after < VALID_AFTER) {
        return;
    }
    for (size_t i = 0; i < s_count; i++) {
        if (!s_alarms[i].enabled) {
            continue;
        }
        time_t when = next_ring(&s_alarms[i], after);
        if (when && (!s_next_due || when < s_next_due)) {
            s_next_due = when;
            s_next_which = i;
        }
    }
}

void alarms_init(void)
{
    nvs_handle_t h;
    if (nvs_open("alarms", NVS_READONLY, &h) == ESP_OK) {
        saved_t saved;
        size_t len = sizeof(saved);
        if (nvs_get_blob(h, "v", &saved, &len) == ESP_OK && len == sizeof(saved) &&
            saved.version == SAVE_VERSION && saved.count <= ALARMS_MAX) {
            s_count = saved.count;
            memcpy(s_alarms, saved.alarms, sizeof(s_alarms));
        }
        nvs_close(h);
    }
    plan(time(NULL));
    ESP_LOGI(TAG, "%u alarms", (unsigned)s_count);
}

size_t alarms_count(void)
{
    return s_count;
}

const alarm_t *alarms_get(size_t i)
{
    return i < s_count ? &s_alarms[i] : NULL;
}

bool alarms_any_enabled(void)
{
    for (size_t i = 0; i < s_count; i++) {
        if (s_alarms[i].enabled) {
            return true;
        }
    }
    return false;
}

static void changed(void)
{
    save();
    plan(time(NULL));
}

bool alarms_add(const alarm_t *a)
{
    if (s_count >= ALARMS_MAX) {
        return false;
    }
    s_alarms[s_count++] = *a;
    changed();
    return true;
}

void alarms_set(size_t i, const alarm_t *a)
{
    if (i < s_count) {
        s_alarms[i] = *a;
        // Turning off (or changing) a snoozed alarm means it shouldn't come back in a few minutes.
        if (i == s_ringing) {
            alarms_stop();
        }
        changed();
    }
}

void alarms_remove(size_t i)
{
    if (i >= s_count) {
        return;
    }
    memmove(&s_alarms[i], &s_alarms[i + 1], (s_count - i - 1) * sizeof(alarm_t));
    s_count--;
    // A snooze would point at the wrong alarm now.
    s_snooze_due = 0;
    s_ringing = SIZE_MAX;
    changed();
}

void alarms_time_changed(void)
{
    plan(time(NULL));
}

static time_t soonest(size_t *which)
{
    time_t due = s_next_due;
    *which = s_next_which;
    if (s_snooze_due && (!due || s_snooze_due < due)) {
        due = s_snooze_due;
        *which = s_ringing;
    }
    return due;
}

TickType_t alarms_ticks_until_next(void)
{
    size_t which;
    time_t due = soonest(&which);
    if (!due) {
        return portMAX_DELAY;
    }
    time_t now = time(NULL);
    if (due <= now) {
        return 0;
    }
    return pdMS_TO_TICKS((uint32_t)(due - now) * 1000);
}

bool alarms_check_due(size_t *which)
{
    size_t i;
    time_t due = soonest(&i);
    time_t now = time(NULL);
    if (!due || now < due) {
        return false;
    }

    if (due == s_snooze_due) {
        s_snooze_due = 0;
    } else {
        // One-time alarms turn themselves off once they ring.
        if (s_alarms[i].days == 0) {
            s_alarms[i].enabled = false;
            save();
        }
        s_auto_snoozes = 0;
        plan(due);
    }
    if (now - due > LATE_LIMIT_S || i >= s_count) {
        ESP_LOGI(TAG, "skipped an alarm that was due %lld s ago", (long long)(now - due));
        return false;
    }
    s_ringing = i;
    *which = i;
    return true;
}

void alarms_snooze(bool automatic)
{
    if (s_ringing >= s_count) {
        return;
    }
    if (automatic && ++s_auto_snoozes > MAX_AUTO_SNOOZES) {
        ESP_LOGI(TAG, "alarm unanswered, giving up");
        alarms_stop();
        return;
    }
    s_snooze_due = time(NULL) + SNOOZE_S;
}

void alarms_stop(void)
{
    s_ringing = SIZE_MAX;
    s_snooze_due = 0;
    s_auto_snoozes = 0;
}

void alarms_format_time(const alarm_t *a, char *buf, size_t len)
{
    if (settings_get()->clock_24h) {
        snprintf(buf, len, "%02d:%02d", a->hour, a->minute);
    } else {
        int h12 = a->hour % 12 == 0 ? 12 : a->hour % 12;
        snprintf(buf, len, "%d:%02d %s", h12, a->minute, a->hour < 12 ? "AM" : "PM");
    }
}

void alarms_format_days(const alarm_t *a, char *buf, size_t len)
{
    static const char *const NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    switch (a->days) {
    case 0: snprintf(buf, len, "Once"); return;
    case 0x7F: snprintf(buf, len, "Every day"); return;
    case 0x3E: snprintf(buf, len, "Weekdays"); return;
    case 0x41: snprintf(buf, len, "Weekends"); return;
    }
    buf[0] = '\0';
    size_t used = 0;
    // Monday first, Sunday last.
    for (int k = 1; k <= 7 && used < len; k++) {
        int d = k % 7;
        if (a->days & (1 << d)) {
            used += snprintf(buf + used, len - used, "%s%s", used ? " " : "", NAMES[d]);
        }
    }
}
