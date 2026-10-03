#include "companion_api.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "alarms.h"
#include "board.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "phone/phone.h"
#include "pmu.h"
#include "settings.h"
#include "steps.h"
#include "ui/notify.h"
#include "weather.h"

#define PROTOCOL_VERSION     1
#define DEVICE_NAME          "LilyGo T-Watch S3"
// Weather older than this is worth replacing; also the gap between repeated requests.
#define WEATHER_STALE_S      (30 * 60)
#define STALE_CHECK_US       (5LL * 60 * 1000 * 1000)

#define GOAL_MIN             1000
#define GOAL_MAX             50000

static const char *TAG = "companion";

static void (*s_on_find)(void);
static bool s_applying_settings;
static int64_t s_last_request_us = INT64_MIN / 2;
static esp_timer_handle_t s_stale_timer;

// ---- Sending ----

static void send_json(cJSON *msg)
{
    char *text = cJSON_PrintUnformatted(msg);
    if (text) {
        phone_companion_send(text);
        cJSON_free(text);
    }
    cJSON_Delete(msg);
}

// A reply to request `id`; takes ownership of `msg`, which already holds the reply's fields.
static void reply(cJSON *msg, const char *type, int id)
{
    cJSON_AddStringToObject(msg, "t", type);
    cJSON_AddNumberToObject(msg, "re", id);
    cJSON_AddBoolToObject(msg, "ok", true);
    send_json(msg);
}

static void reply_error(const char *type, int id, const char *error)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "t", type);
    cJSON_AddNumberToObject(msg, "re", id);
    cJSON_AddBoolToObject(msg, "ok", false);
    cJSON_AddStringToObject(msg, "error", error);
    send_json(msg);
}

void companion_api_request_weather(const char *reason)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"t\":\"weather.request\",\"reason\":\"%s\"}", reason);
    if (phone_companion_send(msg)) {
        s_last_request_us = esp_timer_get_time();
        ESP_LOGI(TAG, "asked for weather (%s)", reason);
    }
}

// Runs on the esp_timer task; only reads a timestamp and queues a message.
static void on_stale_timer(void *arg)
{
    (void)arg;
    if (phone_companion_ready() && weather_age_s() > WEATHER_STALE_S &&
        esp_timer_get_time() - s_last_request_us >= (int64_t)WEATHER_STALE_S * 1000000) {
        companion_api_request_weather("stale");
    }
}

// ---- Reading fields ----

static bool get_number(const cJSON *obj, const char *key, double *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

static int16_t get_int16(const cJSON *obj, const char *key)
{
    double v;
    if (!get_number(obj, key, &v) || v < INT16_MIN + 1 || v > INT16_MAX) {
        return WEATHER_UNKNOWN;
    }
    return (int16_t)lround(v);
}

// Rounded and clamped to 0..max, or -1 if missing.
static int16_t get_level(const cJSON *obj, const char *key, int16_t max)
{
    double v;
    if (!get_number(obj, key, &v)) {
        return -1;
    }
    return (int16_t)(v < 0 ? 0 : (v > max ? max : lround(v)));
}

static int8_t get_percent(const cJSON *obj, const char *key)
{
    return (int8_t)get_level(obj, key, 100);
}

static int64_t get_time(const cJSON *obj, const char *key)
{
    double v;
    return get_number(obj, key, &v) ? (int64_t)v : 0;
}

static uint8_t get_code(const cJSON *obj)
{
    double v;
    return get_number(obj, "code", &v) && v >= 0 && v < 255 ? (uint8_t)v : 255;
}

// Any message may carry the phone's time zone; the clock shifts to true UTC once it's known.
static void read_utc_offset(const cJSON *req)
{
    double v;
    if (!get_number(req, "utc_offset", &v)) {
        return;
    }
    int32_t before = board_utc_offset();
    board_set_utc_offset((int32_t)v);
    if (board_utc_offset() != before) {
        alarms_time_changed();
    }
}

// ---- Handlers ----

static void handle_hello(int id)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddNumberToObject(msg, "protocol", PROTOCOL_VERSION);
    cJSON_AddStringToObject(msg, "name", "K-Watch");
    cJSON_AddStringToObject(msg, "device", DEVICE_NAME);
    cJSON_AddStringToObject(msg, "firmware", esp_app_get_description()->version);
    static const char *const FEATURES[] = {"weather", "settings", "health", "diagnostics", "find"};
    cJSON_AddItemToObject(msg, "features",
                          cJSON_CreateStringArray(FEATURES, sizeof(FEATURES) / sizeof(FEATURES[0])));
    reply(msg, "hello", id);

    if (weather_age_s() > WEATHER_STALE_S) {
        companion_api_request_weather("stale");
    }
}

static void handle_weather_set(const cJSON *req, int id)
{
    const cJSON *in = cJSON_GetObjectItemCaseSensitive(req, "weather");
    if (!cJSON_IsObject(in)) {
        reply_error("weather.set", id, "missing weather");
        return;
    }
    static weather_t w;
    memset(&w, 0, sizeof(w));
    w.updated = get_time(in, "updated");
    if (w.updated == 0) {
        w.updated = time(NULL);
    }
    const cJSON *location = cJSON_GetObjectItemCaseSensitive(in, "location");
    if (cJSON_IsString(location)) {
        snprintf(w.location, sizeof(w.location), "%s", location->valuestring);
    }
    double lat, lon;
    if (get_number(in, "lat", &lat) && get_number(in, "lon", &lon) && fabs(lat) <= 90 && fabs(lon) <= 180) {
        w.has_coords = true;
        w.lat = (float)lat;
        w.lon = (float)lon;
    }
    const cJSON *unit = cJSON_GetObjectItemCaseSensitive(in, "unit");
    w.unit = cJSON_IsString(unit) && unit->valuestring[0] == 'C' ? 'C' : 'F';

    const cJSON *now = cJSON_GetObjectItemCaseSensitive(in, "now");
    w.temp = get_int16(now, "temp");
    w.feels = get_int16(now, "feels");
    w.code = get_code(now);
    const cJSON *day = cJSON_GetObjectItemCaseSensitive(now, "day");
    w.day = !cJSON_IsBool(day) || cJSON_IsTrue(day);
    w.humidity = get_percent(now, "humidity");
    w.wind = get_int16(now, "wind");
    w.uv = (int8_t)get_level(now, "uv", 20);
    w.aqi = get_level(now, "aqi", 999);

    const cJSON *today = cJSON_GetObjectItemCaseSensitive(in, "today");
    w.high = get_int16(today, "high");
    w.low = get_int16(today, "low");
    w.precip = get_percent(today, "precip");
    w.sunrise = get_time(today, "sunrise");
    w.sunset = get_time(today, "sunset");

    const cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(in, "hours"))
    {
        if (w.hour_count == WEATHER_HOURS) {
            break;
        }
        weather_hour_t *h = &w.hours[w.hour_count++];
        h->time = get_time(item, "time");
        h->temp = get_int16(item, "temp");
        h->code = get_code(item);
        h->precip = get_percent(item, "precip");
    }
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(in, "days"))
    {
        if (w.day_count == WEATHER_DAYS) {
            break;
        }
        weather_day_t *d = &w.days[w.day_count++];
        d->time = get_time(item, "time");
        d->high = get_int16(item, "high");
        d->low = get_int16(item, "low");
        d->code = get_code(item);
        d->precip = get_percent(item, "precip");
    }

    weather_set(&w);
    reply(cJSON_CreateObject(), "weather.set", id);
}

static cJSON *settings_json(void)
{
    const settings_t *s = settings_get();
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "brightness", s->brightness);
    cJSON_AddNumberToObject(o, "screen_timeout", s->screen_timeout_s);
    cJSON_AddBoolToObject(o, "raise_to_wake", s->raise_to_wake);
    cJSON_AddBoolToObject(o, "tap_to_wake", s->tap_to_wake);
    cJSON_AddBoolToObject(o, "notify_vibrate", s->notify_vibrate);
    cJSON_AddBoolToObject(o, "touch_feedback", s->touch_feedback);
    cJSON_AddBoolToObject(o, "clock_24h", s->clock_24h);
    cJSON_AddBoolToObject(o, "dnd", s->dnd);
    cJSON_AddBoolToObject(o, "bluetooth", s->bluetooth);
    cJSON_AddNumberToObject(o, "step_goal", steps_goal());
    return o;
}

static void read_bool(const cJSON *in, const char *key, bool *field)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(in, key);
    if (cJSON_IsBool(item)) {
        *field = cJSON_IsTrue(item);
    }
}

static uint8_t nearest_timeout(double seconds)
{
    static const uint8_t STEPS[] = {5, 10, 15, 30};
    uint8_t best = STEPS[0];
    for (size_t i = 0; i < sizeof(STEPS); i++) {
        if (fabs(seconds - STEPS[i]) < fabs(seconds - best)) {
            best = STEPS[i];
        }
    }
    return best;
}

static void handle_settings_set(const cJSON *req, int id)
{
    const cJSON *in = cJSON_GetObjectItemCaseSensitive(req, "settings");
    if (!cJSON_IsObject(in)) {
        reply_error("settings.set", id, "missing settings");
        return;
    }
    settings_t s = *settings_get();
    double v;
    if (get_number(in, "brightness", &v)) {
        s.brightness = (uint8_t)(v < 10 ? 10 : (v > 100 ? 100 : v));
    }
    if (get_number(in, "screen_timeout", &v)) {
        s.screen_timeout_s = nearest_timeout(v);
    }
    read_bool(in, "raise_to_wake", &s.raise_to_wake);
    read_bool(in, "tap_to_wake", &s.tap_to_wake);
    read_bool(in, "notify_vibrate", &s.notify_vibrate);
    read_bool(in, "touch_feedback", &s.touch_feedback);
    read_bool(in, "clock_24h", &s.clock_24h);
    read_bool(in, "dnd", &s.dnd);

    s_applying_settings = true;
    settings_update(&s);
    if (get_number(in, "step_goal", &v)) {
        steps_set_goal((uint32_t)(v < GOAL_MIN ? GOAL_MIN : (v > GOAL_MAX ? GOAL_MAX : v)));
    }
    s_applying_settings = false;

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddItemToObject(msg, "settings", settings_json());
    reply(msg, "settings.set", id);
}

static void handle_health(int id)
{
    cJSON *msg = cJSON_CreateObject();
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char date[16];
    strftime(date, sizeof(date), "%Y-%m-%d", &t);
    cJSON_AddStringToObject(msg, "date", date);
    cJSON_AddNumberToObject(msg, "today", steps_today());
    cJSON_AddNumberToObject(msg, "goal", steps_goal());
    cJSON *days = cJSON_AddArrayToObject(msg, "days");
    for (int d = 1; d <= STEPS_HISTORY_DAYS; d++) {
        uint32_t count;
        cJSON_AddItemToArray(days, steps_on_day(d, &count) ? cJSON_CreateNumber(count) : cJSON_CreateNull());
    }
    reply(msg, "health.get", id);
}

static const char *restart_reason(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "froze";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    default: return "other";
    }
}

static void handle_diag(int id)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "firmware", esp_app_get_description()->version);
    cJSON_AddNumberToObject(msg, "uptime", (double)(esp_timer_get_time() / 1000000));
    cJSON_AddNumberToObject(msg, "time", (double)time(NULL));
    cJSON_AddStringToObject(msg, "restart_reason", restart_reason());
    cJSON_AddNumberToObject(msg, "heap_free", esp_get_free_heap_size());
    cJSON_AddNumberToObject(msg, "heap_min", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(msg, "psram_free", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    pmu_status_t p;
    pmu_get_status(&p);
    cJSON *battery = cJSON_AddObjectToObject(msg, "battery");
    cJSON_AddNumberToObject(battery, "percent", p.battery_percent);
    cJSON_AddNumberToObject(battery, "voltage", p.battery_mv);
    cJSON_AddBoolToObject(battery, "charging", p.charging);
    cJSON_AddBoolToObject(battery, "usb", p.usb_connected);

    cJSON_AddNumberToObject(msg, "notifications", notify_count());
    reply(msg, "diag.get", id);
}

void companion_api_handle(const char *json)
{
    cJSON *req = cJSON_Parse(json);
    if (!cJSON_IsObject(req)) {
        ESP_LOGW(TAG, "not a JSON object: %.40s", json);
        cJSON_Delete(req);
        return;
    }
    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(req, "t");
    const char *type = cJSON_IsString(type_item) ? type_item->valuestring : "";
    double id_value = 0;
    get_number(req, "id", &id_value);
    int id = (int)id_value;
    ESP_LOGI(TAG, "app: %s", type);

    read_utc_offset(req);
    if (strcmp(type, "hello") == 0) {
        handle_hello(id);
    } else if (strcmp(type, "weather.set") == 0) {
        handle_weather_set(req, id);
    } else if (strcmp(type, "settings.get") == 0) {
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddItemToObject(msg, "settings", settings_json());
        reply(msg, "settings.get", id);
    } else if (strcmp(type, "settings.set") == 0) {
        handle_settings_set(req, id);
    } else if (strcmp(type, "health.get") == 0) {
        handle_health(id);
    } else if (strcmp(type, "diag.get") == 0) {
        handle_diag(id);
    } else if (strcmp(type, "find") == 0) {
        if (s_on_find) {
            s_on_find();
        }
        reply(cJSON_CreateObject(), "find", id);
    } else {
        reply_error(type, id, "unknown type");
    }
    cJSON_Delete(req);
}

void companion_api_settings_changed(void)
{
    if (s_applying_settings || !phone_companion_ready()) {
        return;
    }
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "t", "settings.changed");
    cJSON_AddItemToObject(msg, "settings", settings_json());
    send_json(msg);
}

void companion_api_on_find(void (*cb)(void))
{
    s_on_find = cb;
}

void companion_api_init(void)
{
    const esp_timer_create_args_t args = {.callback = on_stale_timer, .name = "weather_stale"};
    if (esp_timer_create(&args, &s_stale_timer) == ESP_OK) {
        esp_timer_start_periodic(s_stale_timer, STALE_CHECK_US);
    }
}
