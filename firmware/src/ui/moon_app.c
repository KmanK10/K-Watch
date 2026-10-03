#include "moon_app.h"

#include <stdio.h>
#include <time.h>

#include "astro.h"
#include "settings.h"
#include "ui/moon_icon.h"
#include "ui/theme.h"
#include "weather.h"

#define CONTENT_W       200
#define DETAIL_ROW_H    26
#define WEEK_DAYS       7
#define ICON_REFRESH_MS (30 * 60 * 1000)

static lv_obj_t *s_page;
static lv_obj_t *s_app_icon;

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

static void format_date(char *buf, size_t size, int64_t when)
{
    time_t t = (time_t)when;
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, size, "%a %b %e", &tm);
}

// Local midnight at the start of the day `days` from today.
static time_t local_midnight(time_t now, int days)
{
    struct tm tm;
    localtime_r(&now, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_mday += days;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

static bool location(double *lat, double *lon)
{
    const weather_t *w = weather_get();
    if (!w || !w->has_coords) {
        return false;
    }
    *lat = w->lat;
    *lon = w->lon;
    return true;
}

static void add_time_column(lv_obj_t *c, int32_t x, const char *name, int64_t when)
{
    lv_obj_t *label = ui_label(c, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_label_set_text(label, name);
    lv_obj_set_pos(label, x, 10);
    char buf[16];
    format_clock(buf, sizeof(buf), when);
    // The moon skips a rise or a set roughly once a month, since it rises about 50 minutes later each day.
    lv_obj_t *value = ui_label(c, when ? &lv_font_montserrat_20 : &lv_font_montserrat_16,
                               when ? lv_color_white() : UI_COLOR_DIM);
    lv_label_set_text(value, when ? buf : "None");
    lv_obj_set_pos(value, x, when ? 28 : 31);
}

static void add_rise_set(time_t now)
{
    lv_obj_t *c = card(62);
    double lat, lon;
    if (!location(&lat, &lon)) {
        lv_obj_t *hint = ui_label(c, &lv_font_montserrat_14, UI_COLOR_DIM);
        lv_obj_set_width(hint, CONTENT_W - 24);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(hint, "Moonrise and moonset need your location from the iPhone app.");
        lv_obj_center(hint);
        return;
    }
    int64_t rise, set;
    astro_moon_times(local_midnight(now, 0), local_midnight(now, 1), lat, lon, &rise, &set);
    add_time_column(c, 0, "Moonrise", rise);
    add_time_column(c, (CONTENT_W - 24) / 2 + 6, "Moonset", set);
}

static void add_week(time_t now, bool south)
{
    lv_obj_t *c = card(66);
    lv_obj_set_style_pad_hor(c, 4, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < WEEK_DAYS; i++) {
        time_t noon = local_midnight(now, i) + 12 * 3600;
        lv_obj_t *col = box(c, 26, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(col, 6, 0);

        struct tm tm;
        localtime_r(&noon, &tm);
        char name[4];
        strftime(name, sizeof(name), "%a", &tm);
        name[2] = '\0';
        lv_obj_t *day = ui_label(col, &lv_font_montserrat_14, i == 0 ? UI_COLOR_ACCENT : UI_COLOR_DIM);
        lv_label_set_text(day, name);

        astro_moon_t m;
        astro_moon(noon, &m);
        moon_icon_set(moon_icon_create(col, 20), m.phase, south);
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

static void add_details(time_t now, const astro_moon_t *m)
{
    lv_obj_t *c = card(16 + 4 * DETAIL_ROW_H);
    char buf[24];
    int row = 0;

    int days = (int)(m->age_days + 0.5f);
    snprintf(buf, sizeof(buf), "%d day%s", days, days == 1 ? "" : "s");
    row = add_detail(c, row, "Age", buf);

    int32_t km = m->distance_km;
    snprintf(buf, sizeof(buf), "%ld,%03ld km", (long)(km / 1000), (long)(km % 1000));
    row = add_detail(c, row, "Distance", buf);

    // The next new and full moons, soonest first.
    int64_t full = astro_next_phase(now, 180);
    int64_t new_moon = astro_next_phase(now, 0);
    bool full_first = full && (full < new_moon || !new_moon);
    for (int i = 0; i < 2; i++) {
        bool is_full = (i == 0) == full_first;
        int64_t when = is_full ? full : new_moon;
        if (!when) {
            continue;
        }
        format_date(buf, sizeof(buf), when);
        row = add_detail(c, row, is_full ? "Full moon" : "New moon", buf);
    }
}

static void build(void)
{
    lv_obj_clean(s_page);
    time_t now = time(NULL);
    double lat = 0, lon = 0;
    bool south = location(&lat, &lon) && lat < 0;
    astro_moon_t m;
    astro_moon(now, &m);

    lv_obj_t *title = ui_label(s_page, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(title, "Moon");

    lv_obj_t *moon = moon_icon_create(s_page, 120);
    moon_icon_set(moon, m.phase, south);

    lv_obj_t *name = ui_label(s_page, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(name, astro_phase_name(m.phase));
    lv_obj_t *lit = ui_label(s_page, &lv_font_montserrat_16, UI_COLOR_DIM);
    lv_label_set_text_fmt(lit, "%d%% illuminated", (int)(m.illumination * 100 + 0.5f));

    add_rise_set(now);
    add_week(now, south);
    add_details(now, &m);
}

static void update_app_icon(void)
{
    if (!s_app_icon) {
        return;
    }
    double lat = 0, lon = 0;
    bool south = location(&lat, &lon) && lat < 0;
    astro_moon_t m;
    astro_moon(time(NULL), &m);
    moon_icon_set(s_app_icon, m.phase, south);
}

static void on_icon_timer(lv_timer_t *t)
{
    (void)t;
    update_app_icon();
}

void moon_app_create(lv_obj_t *parent)
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

void moon_app_on_show(void)
{
    build();
    lv_obj_scroll_to_y(s_page, 0, LV_ANIM_OFF);
    update_app_icon();
}

void moon_app_draw_icon(lv_obj_t *button)
{
    s_app_icon = moon_icon_create(button, 40);
    lv_obj_center(s_app_icon);
    update_app_icon();
    lv_timer_create(on_icon_timer, ICON_REFRESH_MS, NULL);
}
