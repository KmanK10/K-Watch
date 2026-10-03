#pragma once

// iPhone link over Bluetooth LE, using services built into iOS: ANCS for notifications, AMS for
// media control and CTS for the time. These need no phone app. The optional companion app adds
// weather, settings and reports over its own service (docs/companion-protocol.md).

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ANCS category IDs
typedef enum {
    PHONE_CAT_OTHER = 0,
    PHONE_CAT_INCOMING_CALL = 1,
    PHONE_CAT_MISSED_CALL = 2,
    PHONE_CAT_VOICEMAIL = 3,
    PHONE_CAT_SOCIAL = 4,
    PHONE_CAT_SCHEDULE = 5,
    PHONE_CAT_EMAIL = 6,
    // Not in Apple's ANCS spec, but sent for a call in progress; its negative action hangs up.
    PHONE_CAT_ACTIVE_CALL = 12,
} phone_category_t;

typedef struct {
    uint32_t uid;
    uint8_t category;
    bool pre_existing;   // already on the phone when the watch connected
    bool silent;
    bool has_positive;   // e.g. accept a call
    bool has_negative;   // e.g. decline a call, or clear the notification
    char app_id[48];     // bundle ID, e.g. com.apple.MobileSMS
    char app_name[32];   // display name from the phone, e.g. Messages (may be empty)
    char title[64];      // often the sender; iOS uses the app name when the app sets none
    char subtitle[64];   // e.g. an email subject (often empty)
    char message[192];
} phone_notification_t;

typedef struct {
    bool playing;
    uint8_t volume;      // 0-100
    char title[64];
    char artist[48];
} phone_media_t;

typedef enum {
    PHONE_EVT_CONNECTED,
    PHONE_EVT_DISCONNECTED,
    PHONE_EVT_PASSKEY,          // show `passkey` so it can be typed on the phone
    PHONE_EVT_SECURED,          // link encrypted (new pairing or known phone)
    PHONE_EVT_PAIRING_FAILED,
    PHONE_EVT_NOTIFICATION,
    PHONE_EVT_NOTIFICATION_REMOVED,
    PHONE_EVT_TIME,
    PHONE_EVT_MEDIA,
    PHONE_EVT_COMPANION,        // a JSON message from the companion app; free `message` after
} phone_event_type_t;

typedef struct {
    phone_event_type_t type;
    union {
        uint32_t passkey;
        uint32_t uid;
        struct tm time;
        phone_notification_t notification;
        phone_media_t media;
        char *message;
    };
} phone_event_t;

// AMS remote command IDs
typedef enum {
    PHONE_MEDIA_PLAY = 0,
    PHONE_MEDIA_PAUSE = 1,
    PHONE_MEDIA_TOGGLE = 2,
    PHONE_MEDIA_NEXT = 3,
    PHONE_MEDIA_PREVIOUS = 4,
    PHONE_MEDIA_VOLUME_UP = 5,
    PHONE_MEDIA_VOLUME_DOWN = 6,
} phone_media_cmd_t;

// Starts Bluetooth and advertising. Events are posted to `events` (items are phone_event_t)
// from the Bluetooth task.
esp_err_t phone_init(QueueHandle_t events);

// These can be called from any task. They do nothing while the phone is not connected.
void phone_media_command(phone_media_cmd_t cmd);
// positive: accept a call. negative: decline a call or clear the notification on the phone.
void phone_notification_action(uint32_t uid, bool positive);

// Deletes the stored pairing and disconnects, so a phone can pair from scratch.
// The phone also has to forget the watch in its Bluetooth settings.
void phone_forget(void);

// Off disconnects and stops advertising, so the phone can't reconnect until it's back on.
// Can be called before phone_init.
void phone_set_enabled(bool enabled);

// A fast connection while the user is interacting, a slow one otherwise.
void phone_set_interactive(bool interactive);

// Sends a JSON message to the companion app (docs/companion-protocol.md). Any task.
// False if the app isn't connected.
bool phone_companion_send(const char *json);
// True while the companion app is connected and listening.
bool phone_companion_ready(void);
