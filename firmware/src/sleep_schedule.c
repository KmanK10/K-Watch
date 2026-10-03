#include "sleep_schedule.h"

#include <stdbool.h>
#include <time.h>

#include "settings.h"

#define DAY_S (24 * 3600)

static time_t s_last_check;   // 0 until the first check, so boot applies the schedule
static bool s_was_scheduled;
static uint16_t s_last_start;
static uint16_t s_last_end;

// The most recent time at or before `now` that the clock read `minutes` past midnight.
static time_t last_time_at(time_t now, uint16_t minutes)
{
    struct tm t;
    localtime_r(&now, &t);
    t.tm_hour = minutes / 60;
    t.tm_min = minutes % 60;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    time_t at = mktime(&t);
    return at > now ? at - DAY_S : at;
}

void sleep_schedule_check(void)
{
    const settings_t *s = settings_get();
    time_t now = time(NULL);
    bool scheduled = s->sleep_schedule && s->sleep_start != s->sleep_end;
    if (!scheduled) {
        s_was_scheduled = false;
        s_last_check = now;
        return;
    }
    // A schedule that was just set up or changed waits for its next start or end, rather than
    // flipping sleep mode while you're still in Settings.
    if (s_last_check && (!s_was_scheduled || s->sleep_start != s_last_start || s->sleep_end != s_last_end)) {
        s_last_check = now;
    }
    s_was_scheduled = true;
    s_last_start = s->sleep_start;
    s_last_end = s->sleep_end;

    time_t start = last_time_at(now, s->sleep_start);
    time_t end = last_time_at(now, s->sleep_end);
    time_t latest = start > end ? start : end;
    if (latest > s_last_check) {
        bool on = start > end;
        if (on != s->sleep_mode) {
            settings_t changed = *s;
            changed.sleep_mode = on;
            settings_update(&changed);
        }
    }
    s_last_check = now;
}

uint32_t sleep_schedule_seconds_until_next(void)
{
    const settings_t *s = settings_get();
    if (!s->sleep_schedule || s->sleep_start == s->sleep_end) {
        return UINT32_MAX;
    }
    time_t now = time(NULL);
    time_t start = last_time_at(now, s->sleep_start) + DAY_S;
    time_t end = last_time_at(now, s->sleep_end) + DAY_S;
    time_t next = start < end ? start : end;
    return (uint32_t)(next - now) + 1;
}
