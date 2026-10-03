#include "astro.h"

#include <math.h>

// Formulas from the Astronomical Almanac's low-precision sun and moon positions. The large
// angles are reduced in double precision; the trigonometry runs in float, which the ESP32-S3
// does in hardware.

#define PI            3.14159265358979
#define STEP_S        600          // sampling interval for rise and set; crossings are interpolated
#define MAX_SAMPLES   160          // enough for a 25 hour day
#define SYNODIC_DAYS  29.530589f

#define SUN_HORIZON   -0.833f      // refraction plus the sun's radius
#define CIVIL_TWILIGHT -6.0f

typedef float (*altitude_fn)(int64_t t, double lat, double lon);

typedef struct {
    float ra;
    float dec;
} equatorial_t;

typedef struct {
    float lon;
    float lat;
    float parallax;
} moon_pos_t;

static double days_since_j2000(int64_t t)
{
    return (double)t / 86400.0 - 10957.5;
}

static float rad(double deg)
{
    double r = fmod(deg, 360.0);
    if (r < 0) {
        r += 360.0;
    }
    return (float)(r * (PI / 180));
}

static float to_deg(float r)
{
    return r * (float)(180 / PI);
}

static float obliquity(double d)
{
    return rad(23.439 - 0.0000004 * d);
}

static float sun_longitude(double d)
{
    float g = rad(357.528 + 0.9856003 * d);
    return rad(280.460 + 0.9856474 * d + 1.915 * sinf(g) + 0.020 * sinf(2 * g));
}

static moon_pos_t moon_position(double d)
{
    double t = d / 36525.0;
    float a1 = rad(135.0 + 477198.87 * t);
    float a2 = rad(259.3 - 413335.36 * t);
    float a3 = rad(235.7 + 890534.22 * t);
    float a4 = rad(269.9 + 954397.74 * t);
    double lon = 218.32 + 481267.881 * t + 6.29 * sinf(a1) - 1.27 * sinf(a2) + 0.66 * sinf(a3) +
                 0.21 * sinf(a4) - 0.19 * sinf(rad(357.5 + 35999.05 * t)) -
                 0.11 * sinf(rad(186.5 + 966404.03 * t));
    double lat = 5.13 * sinf(rad(93.3 + 483202.02 * t)) + 0.28 * sinf(rad(228.2 + 960400.89 * t)) -
                 0.28 * sinf(rad(318.3 + 6003.15 * t)) - 0.17 * sinf(rad(217.6 - 407332.21 * t));
    double parallax = 0.9508 + 0.0518 * cosf(a1) + 0.0095 * cosf(a2) + 0.0078 * cosf(a3) +
                      0.0028 * cosf(a4);
    return (moon_pos_t){
        .lon = rad(lon),
        .lat = (float)(lat * PI / 180),
        .parallax = (float)(parallax * PI / 180),
    };
}

static equatorial_t to_equatorial(float lon, float lat, float eps)
{
    return (equatorial_t){
        .ra = atan2f(sinf(lon) * cosf(eps) - tanf(lat) * sinf(eps), cosf(lon)),
        .dec = asinf(sinf(lat) * cosf(eps) + cosf(lat) * sinf(eps) * sinf(lon)),
    };
}

// Degrees above the horizon, ignoring refraction.
static float altitude(double d, equatorial_t e, double lat, double lon)
{
    float hour_angle = rad(280.46061837 + 360.98564736629 * d + lon) - e.ra;
    float phi = (float)(lat * PI / 180);
    return to_deg(asinf(sinf(phi) * sinf(e.dec) + cosf(phi) * cosf(e.dec) * cosf(hour_angle)));
}

static float sun_altitude(int64_t t, double lat, double lon)
{
    double d = days_since_j2000(t);
    return altitude(d, to_equatorial(sun_longitude(d), 0, obliquity(d)), lat, lon);
}

// Relative to the altitude at which the moon's upper edge touches the horizon, which allows for
// its parallax, radius and refraction.
static float moon_altitude(int64_t t, double lat, double lon)
{
    double d = days_since_j2000(t);
    moon_pos_t m = moon_position(d);
    float horizon = 0.7275f * to_deg(m.parallax) - 0.5667f;
    return altitude(d, to_equatorial(m.lon, m.lat, obliquity(d)), lat, lon) - horizon;
}

static int sample(altitude_fn f, int64_t start, int64_t end, double lat, double lon, float *out)
{
    int n = 0;
    for (int64_t t = start; t <= end && n < MAX_SAMPLES; t += STEP_S) {
        out[n++] = f(t, lat, lon);
    }
    return n;
}

// The first times the samples rise above and sink below `level`.
static void crossings(const float *s, int n, float level, int64_t start, int64_t *up, int64_t *down)
{
    *up = 0;
    *down = 0;
    for (int i = 1; i < n; i++) {
        float a = s[i - 1] - level;
        float b = s[i] - level;
        if ((a < 0) == (b < 0)) {
            continue;
        }
        int64_t at = start + (int64_t)(i - 1) * STEP_S + (int64_t)(STEP_S * a / (a - b));
        if (b >= 0 && !*up) {
            *up = at;
        } else if (b < 0 && !*down) {
            *down = at;
        }
    }
}

void astro_sun_times(int64_t start, int64_t end, double lat, double lon, astro_sun_t *out)
{
    float s[MAX_SAMPLES];
    int n = sample(sun_altitude, start, end, lat, lon, s);
    crossings(s, n, CIVIL_TWILIGHT, start, &out->dawn, &out->dusk);
    crossings(s, n, SUN_HORIZON, start, &out->sunrise, &out->sunset);
}

astro_light_t astro_light(int64_t t, double lat, double lon)
{
    float alt = sun_altitude(t, lat, lon);
    return alt > SUN_HORIZON ? ASTRO_DAY : (alt > CIVIL_TWILIGHT ? ASTRO_TWILIGHT : ASTRO_NIGHT);
}

bool astro_sun_up(int64_t t, double lat, double lon)
{
    return sun_altitude(t, lat, lon) > SUN_HORIZON;
}

void astro_moon_times(int64_t start, int64_t end, double lat, double lon, int64_t *rise, int64_t *set)
{
    float s[MAX_SAMPLES];
    int n = sample(moon_altitude, start, end, lat, lon, s);
    crossings(s, n, 0, start, rise, set);
}

static float phase_at(int64_t t)
{
    double d = days_since_j2000(t);
    float p = to_deg(moon_position(d).lon - sun_longitude(d));
    return p < 0 ? p + 360 : p;
}

void astro_moon(int64_t t, astro_moon_t *out)
{
    double d = days_since_j2000(t);
    moon_pos_t m = moon_position(d);
    float elongation = m.lon - sun_longitude(d);
    float phase = to_deg(elongation);
    out->phase = phase < 0 ? phase + 360 : phase;
    out->illumination = (1 - cosf(m.lat) * cosf(elongation)) / 2;
    out->age_days = out->phase / 360 * SYNODIC_DAYS;
    out->distance_km = (int32_t)(6378.14f / sinf(m.parallax));
}

// How far past `target` the phase is, from -180 to 180 degrees.
static float phase_offset(int64_t t, float target)
{
    float off = phase_at(t) - target;
    while (off > 180) {
        off -= 360;
    }
    while (off <= -180) {
        off += 360;
    }
    return off;
}

int64_t astro_next_phase(int64_t t, float phase)
{
    const int64_t step = 6 * 3600;
    float prev = phase_offset(t, phase);
    for (int i = 1; i <= 32 * 4; i++) {
        int64_t at = t + i * step;
        float cur = phase_offset(at, phase);
        if (prev < 0 && cur >= 0) {
            return at - step + (int64_t)(step * -prev / (cur - prev));
        }
        prev = cur;
    }
    return 0;
}

const char *astro_phase_name(float phase)
{
    if (phase < 7 || phase >= 353) {
        return "New moon";
    } else if (phase < 83) {
        return "Waxing crescent";
    } else if (phase < 97) {
        return "First quarter";
    } else if (phase < 173) {
        return "Waxing gibbous";
    } else if (phase < 187) {
        return "Full moon";
    } else if (phase < 263) {
        return "Waning gibbous";
    } else if (phase < 277) {
        return "Last quarter";
    }
    return "Waning crescent";
}
