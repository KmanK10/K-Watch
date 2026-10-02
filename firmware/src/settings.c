#include "settings.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

// Bump when the layout of settings_t changes, so old saved settings are ignored.
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
};
static void (*s_on_change)(const settings_t *s);
static bool s_unsaved;

typedef struct {
    uint8_t version;
    settings_t settings;
} saved_t;

void settings_init(void)
{
    nvs_handle_t h;
    if (nvs_open("settings", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    saved_t saved;
    size_t len = sizeof(saved);
    if (nvs_get_blob(h, "v", &saved, &len) == ESP_OK && len == sizeof(saved) &&
        saved.version == SETTINGS_VERSION) {
        s_settings = saved.settings;
    }
    nvs_close(h);
    // Quick toggles start fresh each boot, so a restart always brings the phone link back.
    s_settings.bluetooth = true;
    s_settings.dnd = false;
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
