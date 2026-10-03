#include "settings.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

// Bump when existing fields of settings_t change, so old saved settings are ignored.
// New fields go at the end instead: older, shorter saves load with the new fields at their defaults.
#define SETTINGS_VERSION 2

static const char *TAG = "settings";

static settings_t s_settings = {
    .brightness = 60,
    .screen_timeout_s = 5,
    .raise_to_wake = true,
    .tap_to_wake = true,
    .notify_vibrate = true,
    .clock_24h = false,
    .bluetooth = true,
    .dnd = false,
    .touch_feedback = true,
    .celsius = false,
    .wind_kmh = false,
    .touch_lock = false,
    .sleep_mode = false,
    .sleep_green = false,
    .sleep_schedule = false,
    .sleep_start = 22 * 60,
    .sleep_end = 7 * 60,
};
static void (*s_on_change)(const settings_t *s);
static bool s_unsaved;

// Packed so the settings always start right after the version byte, whatever fields settings_t gains.
typedef struct __attribute__((packed)) {
    uint8_t version;
    settings_t settings;
} saved_t;

void settings_init(void)
{
    nvs_handle_t h;
    if (nvs_open("settings", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    saved_t saved = {.settings = s_settings};
    size_t len = sizeof(saved);
    if (nvs_get_blob(h, "v", &saved, &len) == ESP_OK && len > offsetof(saved_t, settings) &&
        saved.version == SETTINGS_VERSION) {
        s_settings = saved.settings;
    }
    nvs_close(h);
    // Anything out of range falls back to the default, so a bad save can't leave the screen dark.
    if (s_settings.brightness < 10 || s_settings.brightness > 100) {
        s_settings.brightness = 60;
    }
    if (s_settings.screen_timeout_s < 5 || s_settings.screen_timeout_s > 30) {
        s_settings.screen_timeout_s = 5;
    }
    if (s_settings.sleep_start >= 24 * 60 || s_settings.sleep_end >= 24 * 60) {
        s_settings.sleep_start = 22 * 60;
        s_settings.sleep_end = 7 * 60;
    }
    // Quick toggles start fresh each boot, so a restart always brings the phone link back.
    s_settings.bluetooth = true;
    s_settings.dnd = false;
    s_settings.touch_lock = false;
}

const settings_t *settings_get(void)
{
    return &s_settings;
}

void settings_preview(const settings_t *s)
{
    if (memcmp(s, &s_settings, sizeof(*s)) == 0) {
        return;
    }
    s_settings = *s;
    s_unsaved = true;
    if (s_on_change) {
        s_on_change(&s_settings);
    }
}

void settings_update(const settings_t *s)
{
    bool changed = memcmp(s, &s_settings, sizeof(*s)) != 0;
    if (!changed && !s_unsaved) {
        return;
    }
    s_settings = *s;
    s_unsaved = false;

    nvs_handle_t h;
    if (nvs_open("settings", NVS_READWRITE, &h) == ESP_OK) {
        saved_t saved = {.version = SETTINGS_VERSION, .settings = s_settings};
        if (nvs_set_blob(h, "v", &saved, sizeof(saved)) != ESP_OK || nvs_commit(h) != ESP_OK) {
            ESP_LOGW(TAG, "could not save settings");
        }
        nvs_close(h);
    }

    if (changed && s_on_change) {
        s_on_change(&s_settings);
    }
}

void settings_on_change(void (*cb)(const settings_t *s))
{
    s_on_change = cb;
}
