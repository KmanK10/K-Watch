#include "weather.h"

#include <time.h>

#include "esp_log.h"
#include "nvs.h"

// Bump when weather_t changes, so an old saved forecast is ignored instead of misread.
#define WEATHER_VERSION 3

static const char *TAG = "weather";

typedef struct {
    uint8_t version;
    weather_t weather;
} saved_t;

static weather_t s_weather;
static void (*s_on_change)(void);

void weather_init(void)
{
    nvs_handle_t h;
    if (nvs_open("weather", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    static saved_t saved;
    size_t len = sizeof(saved);
    if (nvs_get_blob(h, "v", &saved, &len) == ESP_OK && len == sizeof(saved) &&
        saved.version == WEATHER_VERSION) {
        s_weather = saved.weather;
        ESP_LOGI(TAG, "loaded weather for %s from %ld s ago", s_weather.location, (long)weather_age_s());
    }
    nvs_close(h);
}

const weather_t *weather_get(void)
{
    return s_weather.updated ? &s_weather : NULL;
}

void weather_set(const weather_t *w)
{
    s_weather = *w;
    ESP_LOGI(TAG, "%s: %d%c, code %d", w->location, w->temp, w->unit, w->code);

    nvs_handle_t h;
    if (nvs_open("weather", NVS_READWRITE, &h) == ESP_OK) {
        static saved_t saved;
        saved.version = WEATHER_VERSION;
        saved.weather = s_weather;
        if (nvs_set_blob(h, "v", &saved, sizeof(saved)) == ESP_OK) {
            nvs_commit(h);
        }
        nvs_close(h);
    }
    if (s_on_change) {
        s_on_change();
    }
}

void weather_on_change(void (*cb)(void))
{
    s_on_change = cb;
}

int32_t weather_age_s(void)
{
    if (!s_weather.updated) {
        return INT32_MAX;
    }
    int64_t age = (int64_t)time(NULL) - s_weather.updated;
    return age < 0 ? 0 : (age > INT32_MAX ? INT32_MAX : (int32_t)age);
}

const char *weather_describe(uint8_t code)
{
    switch (code) {
    case 0: return "Clear";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Cloudy";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55: return "Drizzle";
    case 56:
    case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66:
    case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80:
    case 81: return "Showers";
    case 82: return "Heavy showers";
    case 85:
    case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Thunderstorm, hail";
    default: return "Unknown";
    }
}
