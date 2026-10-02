#pragma once

// iPhone link over Bluetooth LE, using only services built into iOS:
// ANCS for notifications and CTS for the time. No phone app is required.

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
} phone_category_t;

typedef struct {
    uint32_t uid;
    uint8_t category;
    char app_id[48];    // bundle ID, e.g. com.apple.MobileSMS
    char title[64];
    char message[192];
} phone_notification_t;

typedef enum {
    PHONE_EVT_CONNECTED,
    PHONE_EVT_DISCONNECTED,
    PHONE_EVT_PASSKEY,          // show `passkey` so it can be typed on the phone
    PHONE_EVT_SECURED,          // link encrypted (new pairing or known phone)
    PHONE_EVT_PAIRING_FAILED,
    PHONE_EVT_NOTIFICATION,
    PHONE_EVT_NOTIFICATION_REMOVED,
    PHONE_EVT_TIME,
} phone_event_type_t;

typedef struct {
    phone_event_type_t type;
    union {
        uint32_t passkey;
        uint32_t uid;
        struct tm time;
        phone_notification_t notification;
    };
} phone_event_t;

// Starts Bluetooth and advertising. Events are posted to `events` (items are phone_event_t)
// from the Bluetooth task.
esp_err_t phone_init(QueueHandle_t events);
