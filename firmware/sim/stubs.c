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

void haptics_play(haptic_pattern_t pattern)
{
    static const char *const NAMES[] = {"tap", "notify", "alert"};
    printf("[buzz] %s\n", NAMES[pattern]);
}
