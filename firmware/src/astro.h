#pragma once

// Sun and moon positions from low-precision formulas, good to a few minutes for rise and set
// times. All times are Unix seconds (UTC). Latitude is north-positive, longitude east-positive.

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t dawn;          // civil twilight starts (sun 6 degrees below the horizon)
    int64_t sunrise;
    int64_t sunset;
    int64_t dusk;          // civil twilight ends
} astro_sun_t;             // each is 0 if it doesn't happen in the window

typedef enum {
    ASTRO_NIGHT,
    ASTRO_TWILIGHT,
    ASTRO_DAY,
} astro_light_t;

typedef struct {
    float phase;           // degrees past new moon: 90 first quarter, 180 full, 270 last quarter
    float illumination;    // 0 to 1
    float age_days;
    int32_t distance_km;
} astro_moon_t;

// The first of each event between `start` and `end` (usually local midnight to midnight).
void astro_sun_times(int64_t start, int64_t end, double lat, double lon, astro_sun_t *out);
astro_light_t astro_light(int64_t t, double lat, double lon);
// True if the sun is up at `t`, for days with no sunrise or sunset.
bool astro_sun_up(int64_t t, double lat, double lon);

void astro_moon(int64_t t, astro_moon_t *out);
void astro_moon_times(int64_t start, int64_t end, double lat, double lon, int64_t *rise, int64_t *set);
// The next time after `t` the moon reaches `phase` degrees (0 new, 180 full), or 0 if not found.
int64_t astro_next_phase(int64_t t, float phase);
const char *astro_phase_name(float phase);
