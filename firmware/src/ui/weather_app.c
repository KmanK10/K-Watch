#include "weather_app.h"

#include <stdio.h>
#include <time.h>

#include "astro.h"
#include "settings.h"
#include "ui/theme.h"
#include "ui/weather_icon.h"
#include "weather.h"

#define CONTENT_W      200
#define HOUR_COLUMNS   6
#define DETAIL_ROW_H   26
#define DAY_ROW_H      32
#define PRECIP_SHOWN   10      // below this chance, rain isn't worth mentioning

#define COLOR_PRECIP   lv_color_hex(0x42A5F5)

static lv_obj_t *s_page;
static lv_obj_t *s_updated;
static void (*s_on_refresh)(void);

static lv_obj_t *box(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_scrollable(b, false);
    lv_obj_set_clickable(b, false);
    return b;
}

static lv_obj_t *card(int32_t h)
{
    lv_obj_t *c = box(s_page, CONTENT_W, h);
    lv_obj_set_style_bg_color(c, UI_COLOR_BUTTON, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_pad_hor(c, 12, 0);
    return c;
}

static void format_age(char *buf, size_t size, int32_t age_s)
{
    if (age_s < 60) {
        snprintf(buf, size, "Updated just now");
    } else if (age_s < 3600) {
        snprintf(buf, size, "Updated %ld min ago", (long)(age_s / 60));
    } else if (age_s < 86400) {
        snprintf(buf, size, "Updated %ld h ago", (long)(age_s / 3600));
    } else {
        snprintf(buf, size, "Updated %ld days ago", (long)(age_s / 86400));
    }
}

static void format_hour(char *buf, size_t size, int64_t when)
{
    time_t t = (time_t)when;
    struct tm tm;
    localtime_r(&t, &tm);
    if (settings_get()->clock_24h) {
        snprintf(buf, size, "%02d", tm.tm_hour);
    } else {
        int h = tm.tm_hour % 12 == 0 ? 12 : tm.tm_hour % 12;
        snprintf(buf, size, "%d%s", h, tm.tm_hour < 12 ? "a" : "p");
    }
}

static void on_refresh(lv_event_t *e)
{
    (void)e;
    lv_label_set_text(s_updated, "Asking iPhone...");
    if (s_on_refresh) {
        s_on_refresh();
    }
}

static void add_refresh_row(const weather_t *w)
{
    lv_obj_t *row = box(s_page, CONTENT_W, 44);
    s_updated = ui_label(row, &lv_font_montserrat_14, UI_COLOR_DIM);
    if (w) {
        char buf[32];
        format_age(buf, sizeof(buf), weather_age_s());
        lv_label_set_text(s_updated, buf);
    } else {
        lv_label_set_text(s_updated, "Tap to ask iPhone");
    }
    lv_obj_align(s_updated, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *btn = ui_round_button(row, LV_SYMBOL_REFRESH, 40, UI_COLOR_BUTTON);
    lv_obj_set_gesture_bubble(btn, true);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(btn, on_refresh, LV_EVENT_CLICKED, NULL);
}

static void build_empty(void)
{
    lv_obj_t *icon = weather_icon_create(s_page, 72);
    weather_icon_set(icon, 2, true);
    lv_obj_t *title = ui_label(s_page, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(title, "No weather yet");
    lv_obj_t *hint = ui_label(s_page, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_obj_set_width(hint, CONTENT_W);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint, "Open the K-Watch app on your iPhone to send the forecast.");
    add_refresh_row(NULL);
}

static void add_hero(const weather_t *w)
{
    lv_obj_t *location = ui_label(s_page, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(location, w->location[0] ? w->location : "Weather");

    lv_obj_t *hero = box(s_page, CONTENT_W, 76);
    lv_obj_t *icon = weather_icon_create(hero, 72);
    weather_icon_set(icon, w->code, w->day);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *temp = weather_temp_create(hero, &lv_font_montserrat_48, lv_color_white());
    weather_temp_set(temp, w->temp);
    lv_obj_align(temp, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_obj_t *desc = ui_label(s_page, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_text(desc, weather_describe(w->code));

    if (w->high != WEATHER_UNKNOWN && w->low != WEATHER_UNKNOWN) {
        lv_obj_t *range = ui_label(s_page, &lv_font_montserrat_16, UI_COLOR_DIM);
        lv_label_set_text_fmt(range, "H %d   L %d", w->high, w->low);
    }
}

static void add_hours(const weather_t *w)
{
    if (w->hour_count == 0) {
        return;
    }
    lv_obj_t *c = card(86);
    lv_obj_set_style_pad_hor(c, 4, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    int n = w->hour_count < HOUR_COLUMNS ? w->hour_count : HOUR_COLUMNS;
    for (int i = 0; i < n; i++) {
        const weather_hour_t *h = &w->hours[i];
        lv_obj_t *col = box(c, 30, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(col, 4, 0);

        char buf[8];
        format_hour(buf, sizeof(buf), h->time);
        lv_obj_t *time = ui_label(col, &lv_font_montserrat_14, UI_COLOR_DIM);
        lv_label_set_text(time, buf);
        // Hours are drawn as day or night from the sunrise and sunset, when known.
        bool day = w->sunrise && w->sunset ? h->time >= w->sunrise && h->time < w->sunset : w->day;
        weather_icon_set(weather_icon_create(col, 24), h->code, day);
        lv_obj_t *temp = ui_label(col, &lv_font_montserrat_14, lv_color_white());
        if (h->temp == WEATHER_UNKNOWN) {
            lv_label_set_text(temp, "--");
        } else {
            lv_label_set_text_fmt(temp, "%d", h->temp);
        }
    }
}

static int add_detail(lv_obj_t *c, int row, const char *name, const char *value)
{
    lv_obj_t *label = ui_label(c, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_text(label, name);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 8 + row * DETAIL_ROW_H);
    lv_obj_t *v = ui_label(c, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(v, value);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, 0, 8 + row * DETAIL_ROW_H);
    return row + 1;
}

static void add_details(const weather_t *w)
{
    int rows = (w->feels != WEATHER_UNKNOWN) + (w->humidity >= 0) + (w->wind != WEATHER_UNKNOWN) +
               (w->precip >= 0);
    if (rows == 0) {
        return;
    }
    lv_obj_t *c = card(16 + rows * DETAIL_ROW_H);
    char buf[16];
    int row = 0;
    if (w->feels != WEATHER_UNKNOWN) {
        snprintf(buf, sizeof(buf), "%d", w->feels);
        row = add_detail(c, row, "Feels like", buf);
    }
    if (w->humidity >= 0) {
        snprintf(buf, sizeof(buf), "%d%%", w->humidity);
        row = add_detail(c, row, "Humidity", buf);
    }
    if (w->wind != WEATHER_UNKNOWN) {
        snprintf(buf, sizeof(buf), "%d %s", w->wind, w->unit == 'C' ? "km/h" : "mph");
        row = add_detail(c, row, "Wind", buf);
    }
    if (w->precip >= 0) {
        snprintf(buf, sizeof(buf), "%d%%", w->precip);
        add_detail(c, row, "Rain chance", buf);
    }
}

typedef struct {
    int16_t at;
    uint32_t color;
} stop_t;

typedef struct {
    int16_t upto;          // highest value in this level
    const char *name;
    uint32_t color;
} level_t;

typedef struct {
    const char *name;
    int16_t max;           // the end of the bar; higher values pin the marker there
    const stop_t *stops;
    int stop_count;
    const level_t *levels;
    int level_count;
} gauge_t;

#define GREEN  0x4CAF50
#define YELLOW 0xFFD600
#define ORANGE 0xFF9800
#define RED    0xF44336
#define PURPLE 0xAB47BC
#define MAROON 0x9E1B4B

static const stop_t UV_STOPS[] = {{0, GREEN}, {3, YELLOW}, {6, ORANGE}, {8, RED}, {11, PURPLE}};
static const level_t UV_LEVELS[] = {
    {2, "Low", GREEN}, {5, "Moderate", YELLOW}, {7, "High", ORANGE},
    {10, "Very high", RED}, {INT16_MAX, "Extreme", PURPLE},
};
static const gauge_t UV_GAUGE = {"UV index", 11, UV_STOPS, 5, UV_LEVELS, 5};

static const stop_t AQI_STOPS[] = {{0, GREEN}, {50, YELLOW}, {100, ORANGE}, {150, RED}, {200, PURPLE}, {300, MAROON}};
static const level_t AQI_LEVELS[] = {
    {50, "Good", GREEN}, {100, "Moderate", YELLOW}, {150, "Sensitive groups", ORANGE},
    {200, "Unhealthy", RED}, {300, "Very unhealthy", PURPLE}, {INT16_MAX, "Hazardous", MAROON},
};
static const gauge_t AQI_GAUGE = {"AQI", 300, AQI_STOPS, 6, AQI_LEVELS, 6};

#define GAUGE_PITCH 48
#define GAUGE_BAR_W 176
#define GAUGE_BAR_H 8
#define MARKER_SIZE 14

// A full-width bar at `y`, filled with bar_segment. The rounded ends come from clipping the
// segments to the bar's corners.
static lv_obj_t *bar_create(lv_obj_t *c, int32_t y)
{
    lv_obj_t *bar = box(c, GAUGE_BAR_W, GAUGE_BAR_H);
    lv_obj_set_pos(bar, 0, y);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(bar, true, 0);
    return bar;
}

// Fills x1..x2 of the bar, fading from `from` to `to`.
static void bar_segment(lv_obj_t *bar, int32_t x1, int32_t x2, uint32_t from, uint32_t to)
{
    lv_obj_t *seg = box(bar, x2 - x1, GAUGE_BAR_H);
    lv_obj_set_pos(seg, x1, 0);
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(seg, lv_color_hex(from), 0);
    if (to != from) {
        lv_obj_set_style_bg_grad_color(seg, lv_color_hex(to), 0);
        lv_obj_set_style_bg_grad_dir(seg, LV_GRAD_DIR_HOR, 0);
    }
}

// A white dot on the bar at `y`, centred on `x` but kept within the bar's ends.
static void bar_marker(lv_obj_t *c, int32_t y, int32_t x)
{
    lv_obj_t *marker = box(c, MARKER_SIZE, MARKER_SIZE);
    int32_t mx = x - MARKER_SIZE / 2;
    mx = mx < 0 ? 0 : (mx > GAUGE_BAR_W - MARKER_SIZE ? GAUGE_BAR_W - MARKER_SIZE : mx);
    lv_obj_set_pos(marker, mx, y + GAUGE_BAR_H / 2 - MARKER_SIZE / 2);
    lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(marker, lv_color_white(), 0);
    lv_obj_set_style_border_width(marker, 3, 0);
    lv_obj_set_style_border_color(marker, UI_COLOR_BUTTON, 0);
}

static int32_t gauge_x(const gauge_t *g, int16_t value)
{
    int16_t v = value > g->max ? g->max : value;
    return (int32_t)v * GAUGE_BAR_W / g->max;
}

static void add_gauge(lv_obj_t *c, int row, const gauge_t *g, int16_t value)
{
    int32_t y = 10 + row * GAUGE_PITCH;
    lv_obj_t *name = ui_label(c, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_text(name, g->name);
    lv_obj_set_pos(name, 0, y);

    const level_t *level = &g->levels[g->level_count - 1];
    for (int i = 0; i < g->level_count; i++) {
        if (value <= g->levels[i].upto) {
            level = &g->levels[i];
            break;
        }
    }
    lv_obj_t *v = ui_label(c, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_recolor(v, true);
    lv_label_set_text_fmt(v, "%d  #%06lX %s#", value, (unsigned long)level->color, level->name);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, 0, y);

    lv_obj_t *bar = bar_create(c, y + 26);
    for (int i = 0; i + 1 < g->stop_count; i++) {
        bar_segment(bar, gauge_x(g, g->stops[i].at), gauge_x(g, g->stops[i + 1].at),
                    g->stops[i].color, g->stops[i + 1].color);
    }
    bar_marker(c, y + 26, gauge_x(g, value));
}

static void add_gauges(const weather_t *w)
{
    int rows = (w->uv >= 0) + (w->aqi >= 0);
    if (rows == 0) {
        return;
    }
    lv_obj_t *c = card(rows * GAUGE_PITCH + 10);
    int row = 0;
    if (w->uv >= 0) {
        add_gauge(c, row++, &UV_GAUGE, w->uv);
    }
    if (w->aqi >= 0) {
        add_gauge(c, row, &AQI_GAUGE, w->aqi);
    }
}

#define SUN_CARD_H     160
#define COLOR_NIGHT    0x2A3A7A
#define COLOR_TWILIGHT 0xFF8A65
#define COLOR_DAY      0xFFD54F

static const uint32_t LIGHT_COLORS[] = {
    [ASTRO_NIGHT] = COLOR_NIGHT,
    [ASTRO_TWILIGHT] = COLOR_TWILIGHT,
    [ASTRO_DAY] = COLOR_DAY,
};

static void format_clock(char *buf, size_t size, int64_t when)
{
    if (!when) {
        snprintf(buf, size, "--");
        return;
    }
    time_t t = (time_t)when;
    struct tm tm;
    localtime_r(&t, &tm);
    if (settings_get()->clock_24h) {
        snprintf(buf, size, "%02d:%02d", tm.tm_hour, tm.tm_min);
    } else {
        int h = tm.tm_hour % 12 == 0 ? 12 : tm.tm_hour % 12;
        snprintf(buf, size, "%d:%02d %s", h, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
    }
}

static void add_sun_time(lv_obj_t *c, int col, int row, const char *name, uint32_t color, int64_t when)
{
    int32_t x = col * (GAUGE_BAR_W / 2 + 6);
    int32_t y = 74 + row * 42;
    lv_obj_t *label = ui_label(c, &lv_font_montserrat_14, lv_color_hex(color));
    lv_label_set_text(label, name);
    lv_obj_set_pos(label, x, y);
    char buf[16];
    format_clock(buf, sizeof(buf), when);
    lv_obj_t *value = ui_label(c, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_text(value, buf);
    lv_obj_set_pos(value, x, y + 16);
}

// Today from midnight to midnight: night, twilight and day along a bar, with the times below.
static void add_sun(const weather_t *w)
{
    if (!w->has_coords) {
        return;
    }
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    time_t start = mktime(&tm);
    tm.tm_mday++;
    tm.tm_isdst = -1;
    time_t end = mktime(&tm);
    astro_sun_t sun;
    astro_sun_times(start, end, w->lat, w->lon, &sun);

    lv_obj_t *c = card(SUN_CARD_H);
    lv_obj_t *title = ui_label(c, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_text(title, "Daylight");
    lv_obj_set_pos(title, 0, 10);
    lv_obj_t *length = ui_label(c, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    if (sun.sunrise && sun.sunset && sun.sunset > sun.sunrise) {
        int32_t mins = (int32_t)((sun.sunset - sun.sunrise) / 60);
        lv_label_set_text_fmt(length, "%ldh %02ldm", (long)(mins / 60), (long)(mins % 60));
    } else if (!sun.sunrise && !sun.sunset) {
        lv_label_set_text(length, astro_sun_up(start + (end - start) / 2, w->lat, w->lon) ? "All day" : "None");
    } else {
        lv_label_set_text(length, "");
    }
    lv_obj_align(length, LV_ALIGN_TOP_RIGHT, 0, 10);

    // Each pixel of the bar is coloured by the light at the middle of its slice of the day.
    lv_obj_t *bar = bar_create(c, 38);
    int run_start = 0;
    int run_light = -1;
    for (int x = 0; x <= GAUGE_BAR_W; x++) {
        int light = -1;
        if (x < GAUGE_BAR_W) {
            int64_t t = start + (int64_t)(end - start) * (2 * x + 1) / (2 * GAUGE_BAR_W);
            light = (int)astro_light(t, w->lat, w->lon);
        }
        if (light != run_light) {
            if (run_light >= 0) {
                bar_segment(bar, run_start, x, LIGHT_COLORS[run_light], LIGHT_COLORS[run_light]);
            }
            run_start = x;
            run_light = light;
        }
    }
    bar_marker(c, 38, (int32_t)((int64_t)(now - start) * GAUGE_BAR_W / (end - start)));

    static const char *const HOURS_12[] = {"6a", "12p", "6p"};
    static const char *const HOURS_24[] = {"06", "12", "18"};
    bool h24 = settings_get()->clock_24h;
    for (int i = 0; i < 3; i++) {
        lv_obj_t *hour = ui_label(c, &lv_font_montserrat_14, UI_COLOR_DIM);
        lv_label_set_text(hour, h24 ? HOURS_24[i] : HOURS_12[i]);
        lv_obj_update_layout(hour);
        lv_obj_set_pos(hour, GAUGE_BAR_W * (i + 1) / 4 - lv_obj_get_width(hour) / 2, 50);
    }

    add_sun_time(c, 0, 0, "Dawn", COLOR_TWILIGHT, sun.dawn);
    add_sun_time(c, 1, 0, "Sunrise", COLOR_DAY, sun.sunrise);
    add_sun_time(c, 0, 1, "Sunset", COLOR_DAY, sun.sunset);
    add_sun_time(c, 1, 1, "Dusk", COLOR_TWILIGHT, sun.dusk);
}

static void add_days(const weather_t *w)
{
    for (int i = 0; i < w->day_count; i++) {
        const weather_day_t *d = &w->days[i];
        lv_obj_t *row = box(s_page, CONTENT_W, DAY_ROW_H);

        char name[8];
        if (i == 0) {
            snprintf(name, sizeof(name), "Today");
        } else {
            time_t t = (time_t)d->time;
            struct tm tm;
            localtime_r(&t, &tm);
            strftime(name, sizeof(name), "%a", &tm);
        }
        lv_obj_t *day = ui_label(row, &lv_font_montserrat_16, lv_color_white());
        lv_label_set_text(day, name);
        lv_obj_align(day, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *icon = weather_icon_create(row, 26);
        weather_icon_set(icon, d->code, true);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 58, 0);

        if (d->precip >= PRECIP_SHOWN) {
            lv_obj_t *precip = ui_label(row, &lv_font_montserrat_14, COLOR_PRECIP);
            lv_label_set_text_fmt(precip, "%d%%", d->precip);
            lv_obj_align(precip, LV_ALIGN_LEFT_MID, 90, 0);
        }

        lv_obj_t *range = ui_label(row, &lv_font_montserrat_16, lv_color_white());
        if (d->high != WEATHER_UNKNOWN && d->low != WEATHER_UNKNOWN) {
            lv_label_set_text_fmt(range, "%d  #9E9E9E %d#", d->high, d->low);
            lv_label_set_recolor(range, true);
        } else {
            lv_label_set_text(range, "--");
        }
        lv_obj_align(range, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

static void build(void)
{
    lv_obj_clean(s_page);
    const weather_t *w = weather_get();
    if (!w) {
        build_empty();
        return;
    }
    add_hero(w);
    add_hours(w);
    add_details(w);
    add_gauges(w);
    add_days(w);
    add_sun(w);
    add_refresh_row(w);
}

void weather_app_create(lv_obj_t *parent)
{
    s_page = parent;
    lv_obj_set_scrollable(s_page, true);
    lv_obj_set_scroll_dir(s_page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(s_page, 14, 0);
    lv_obj_set_style_pad_bottom(s_page, 16, 0);
    lv_obj_set_style_pad_row(s_page, 8, 0);
    lv_obj_set_flex_flow(s_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

void weather_app_on_show(void)
{
    build();
    lv_obj_scroll_to_y(s_page, 0, LV_ANIM_OFF);
}

void weather_app_refresh(void)
{
    if (s_page && lv_screen_active() == lv_obj_get_screen(s_page)) {
        build();
    }
}

void weather_app_on_refresh(void (*cb)(void))
{
    s_on_refresh = cb;
}
