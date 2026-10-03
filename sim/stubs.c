// Stand-ins for the watch's hardware and phone link, printing what would have happened.

#include <stdio.h>

#include "haptics.h"
#include "phone/phone.h"
#include "sim.h"
#include "steps.h"
#include "ui/notify.h"

static const char *const MEDIA_NAMES[] = {"play", "pause", "play/pause", "next", "previous",
                                          "volume up", "volume down"};

void phone_media_command(phone_media_cmd_t cmd)
{
    printf("[phone] media: %s\n", MEDIA_NAMES[cmd]);
    sim_on_media_command(cmd);
}

void phone_notification_action(uint32_t uid, bool positive)
{
    printf("[phone] notification %u: %s\n", (unsigned)uid, positive ? "accept" : "decline/clear");
    // The phone would answer by removing the notification.
    notify_remove(uid);
}

// A made-up month: yesterday first, 0 for days the watch was off.
static const uint32_t SAMPLE_DAYS[STEPS_HISTORY_DAYS] = {
    9120, 6480, 11200, 0, 7350, 8800, 5210, 10400, 7900, 6600, 8300, 9900, 4500, 7200, 8100,
};
static uint32_t s_goal = 8000;

uint32_t steps_today(void)
{
    return 4321;
}

bool steps_on_day(int days_ago, uint32_t *count)
{
    if (days_ago == 0) {
        *count = steps_today();
        return true;
    }
    if (days_ago > STEPS_HISTORY_DAYS || SAMPLE_DAYS[days_ago - 1] == 0) {
        return false;
    }
    *count = SAMPLE_DAYS[days_ago - 1];
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
    printf("[steps] goal %u\n", (unsigned)goal);
    s_goal = goal;
}

void phone_forget(void)
{
    printf("[phone] pairing deleted, disconnecting\n");
}

void phone_set_enabled(bool enabled)
{
    printf("[phone] Bluetooth %s\n", enabled ? "on" : "off");
}

static bool s_touch_feedback = true;

void haptics_play(haptic_pattern_t pattern)
{
    static const char *const NAMES[] = {
        [HAPTIC_TAP] = "tap",
        [HAPTIC_TICK] = "tick",
        [HAPTIC_NOTIFY] = "notify",
        [HAPTIC_ALERT] = "alert",
    };
    if (!s_touch_feedback && (pattern == HAPTIC_TAP || pattern == HAPTIC_TICK)) {
        return;
    }
    printf("[buzz] %s\n", NAMES[pattern]);
}

void haptics_set_touch_feedback(bool on)
{
    s_touch_feedback = on;
}
