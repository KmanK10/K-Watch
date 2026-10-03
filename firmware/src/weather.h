#pragma once

// The latest weather sent by the companion app, kept in flash so it survives a restart.
// weather_get converts it to the units chosen with weather_set_units; `unit` and `wind_kmh`
// say which units a weather_t is in.

#include <stdbool.h>
#include <stdint.h>

#define WEATHER_HOURS     12
#define WEATHER_DAYS      7
// For temperatures and other values the app didn't send.
#define WEATHER_UNKNOWN   INT16_MIN

typedef struct {
    int64_t time;
    int16_t temp;
    uint8_t code;          // WMO weather code
    int8_t precip;         // chance in percent, -1 if unknown
} weather_hour_t;

typedef struct {
    int64_t time;          // start of the day
    int16_t high;
    int16_t low;
    uint8_t code;
    int8_t precip;
} weather_day_t;

typedef struct {
    int64_t updated;       // when the app fetched it; 0 means no weather yet
    char location[32];
    bool has_coords;       // lat/lon are known, for the sun and moon times
    float lat;
    float lon;
    char unit;             // 'F' or 'C'
    int16_t temp;
    int16_t feels;
    uint8_t code;
    bool day;              // false at night
    int8_t humidity;       // percent, -1 if unknown
    bool wind_kmh;         // `wind` is in km/h; mph when false
    int16_t wind;
    int16_t high;
    int16_t low;
    int8_t precip;
    int8_t uv;             // UV index, rounded; -1 if unknown
    int16_t aqi;           // US AQI; -1 if unknown
    int64_t sunrise;
    int64_t sunset;
    uint8_t hour_count;
    uint8_t day_count;
    weather_hour_t hours[WEATHER_HOURS];
    weather_day_t days[WEATHER_DAYS];
} weather_t;

void weather_init(void);

// NULL until the app has sent weather at least once.
const weather_t *weather_get(void);

// The units weather_get returns. Tells the change listener if they changed.
void weather_set_units(bool celsius, bool wind_kmh);

// Saves it and tells the change listener. Call from the main task.
void weather_set(const weather_t *w);
void weather_on_change(void (*cb)(void));

// Seconds since the weather was fetched, or INT32_MAX if there is none.
int32_t weather_age_s(void);

// A short description of a WMO weather code, e.g. "Light rain".
const char *weather_describe(uint8_t code);
