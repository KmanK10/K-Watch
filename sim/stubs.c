// Stand-ins for the watch's hardware and phone link, printing what would have happened.

#include <stdio.h>

#include "haptics.h"
#include "phone/phone.h"
#include "sim.h"
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
