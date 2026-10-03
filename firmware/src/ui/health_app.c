#include "health_app.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "steps.h"
#include "ui/screens.h"
#include "ui/theme.h"

#define CONTENT_W      200
#define RING_SIZE      180
#define RING_W         14
#define ARC_MAX        1000

#define CHART_H        96
#define BAR_MAX_H      64
#define BAR_W          16
#define CHART_DAYS     7

#define GOAL_STEP      500
#define GOAL_MIN       1000
#define GOAL_MAX       50000

#define COLOR_GOAL_MET lv_color_hex(0x2EBD59)
#define COLOR_NO_DATA  lv_color_hex(0x5A5A5A)

static lv_obj_t *s_page;
static lv_obj_t *s_ring;
static lv_obj_t *s_today;
static lv_obj_t *s_of_goal;
static lv_obj_t *s_week_avg;
static lv_obj_t *s_month_avg;
static lv_obj_t *s_bars[CHART_DAYS];       // oldest first, today last
static lv_obj_t *s_bar_days[CHART_DAYS];
static lv_obj_t *s_goal_line;
static lv_obj_t *s_goal;

// 12345 -> "12,345"
static void format_steps(char *buf, size_t size, uint32_t n)
{
    if (n >= 1000) {
        snprintf(buf, size, "%lu,%03lu", (unsigned long)(n / 1000), (unsigned long)(n % 1000));
    } else {
        snprintf(buf, size, "%lu", (unsigned long)n);
    }
}

static void show_average(lv_obj_t *label, int days)
{
    int with_data;
    uint32_t avg = steps_average(days, &with_data);
    char buf[16];
    if (with_data == 0) {
        lv_label_set_text(label, "--");
        return;
    }
    format_steps(buf, sizeof(buf), avg);
    lv_label_set_text(label, buf);
}

static void refresh_chart(uint32_t goal)
{
    uint32_t counts[CHART_DAYS];
    bool have[CHART_DAYS];
    uint32_t top = goal;
    for (int c = 0; c < CHART_DAYS; c++) {
        have[c] = steps_on_day(CHART_DAYS - 1 - c, &counts[c]);
        if (have[c] && counts[c] > top) {
            top = counts[c];
        }
    }

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    static const char DAY_LETTERS[] = "SMTWTFS";

    for (int c = 0; c < CHART_DAYS; c++) {
        int32_t h = have[c] ? (int32_t)((uint64_t)counts[c] * BAR_MAX_H / top) : 0;
        // A sliver, so a day with no steps still shows where its bar would be.
        lv_obj_set_height(s_bars[c], h < 3 ? 3 : h);
        lv_color_t color = !have[c] ? COLOR_NO_DATA : counts[c] >= goal ? COLOR_GOAL_MET : UI_COLOR_ACCENT;
        lv_obj_set_style_bg_color(s_bars[c], color, 0);

        int wday = (t.tm_wday - (CHART_DAYS - 1 - c) + 7 * CHART_DAYS) % 7;
        char letter[2] = {DAY_LETTERS[wday], '\0'};
        lv_label_set_text(s_bar_days[c], letter);
    }
    lv_obj_set_y(s_goal_line, BAR_MAX_H - (int32_t)((uint64_t)goal * BAR_MAX_H / top));
}

static void refresh(void)
{
    uint32_t today = steps_today();
    uint32_t goal = steps_goal();
    char buf[32];

    format_steps(buf, sizeof(buf), today);
    lv_label_set_text(s_today, buf);
    format_steps(buf, sizeof(buf), goal);
    lv_label_set_text_fmt(s_of_goal, "of %s", buf);
    lv_label_set_text(s_goal, buf);

    uint32_t progress = goal ? (uint32_t)((uint64_t)today * ARC_MAX / goal) : ARC_MAX;
    lv_arc_set_value(s_ring, progress > ARC_MAX ? ARC_MAX : (int32_t)progress);
    lv_obj_set_style_arc_color(s_ring, today >= goal ? COLOR_GOAL_MET : UI_COLOR_ACCENT, LV_PART_INDICATOR);

    show_average(s_week_avg, 7);
    show_average(s_month_avg, STEPS_HISTORY_DAYS);
    refresh_chart(goal);
}

static void on_timer(lv_timer_t *t)
{
    (void)t;
    if (lv_screen_active() == lv_obj_get_screen(s_page)) {
        refresh();
    }
}

static void on_goal(lv_event_t *e)
{
    int32_t goal = (int32_t)steps_goal() + (int32_t)(intptr_t)lv_event_get_user_data(e);
    goal = goal < GOAL_MIN ? GOAL_MIN : (goal > GOAL_MAX ? GOAL_MAX : goal);
    steps_set_goal((uint32_t)goal);
    refresh();
}

static lv_obj_t *add_card(int32_t height)
{
    lv_obj_t *card = lv_obj_create(s_page);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, CONTENT_W, height);
    lv_obj_set_style_bg_color(card, UI_COLOR_BUTTON, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_hor(card, 12, 0);
    lv_obj_set_scrollable(card, false);
    return card;
}

static lv_obj_t *add_average_row(lv_obj_t *card, const char *name, int32_t y)
{
    lv_obj_t *label = ui_label(card, &lv_font_montserrat_16, lv_color_white());
    lv_label_set_text(label, name);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_t *value = ui_label(card, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_obj_align(value, LV_ALIGN_TOP_RIGHT, 0, y);
    return value;
}

static void create_ring(void)
{
    lv_obj_t *box = lv_obj_create(s_page);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, RING_SIZE, RING_SIZE);
    lv_obj_set_scrollable(box, false);

    s_ring = lv_arc_create(box);
    lv_obj_set_size(s_ring, RING_SIZE, RING_SIZE);
    lv_obj_center(s_ring);
    lv_arc_set_rotation(s_ring, 270);
    lv_arc_set_bg_angles(s_ring, 0, 360);
    lv_arc_set_range(s_ring, 0, ARC_MAX);
    lv_obj_remove_style(s_ring, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(s_ring, false);
    lv_obj_set_style_arc_width(s_ring, RING_W, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_ring, RING_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_ring, UI_COLOR_BUTTON, LV_PART_MAIN);

    lv_obj_t *caption = ui_label(box, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_label_set_text(caption, "Steps today");
    lv_obj_align(caption, LV_ALIGN_CENTER, 0, -38);

    s_today = ui_label(box, &lv_font_montserrat_48, lv_color_white());
    lv_obj_align(s_today, LV_ALIGN_CENTER, 0, 0);

    s_of_goal = ui_label(box, &lv_font_montserrat_16, UI_COLOR_DIM);
    lv_obj_align(s_of_goal, LV_ALIGN_CENTER, 0, 38);
}

static void create_chart(void)
{
    lv_obj_t *card = add_card(CHART_H + 34);
    lv_obj_t *title = ui_label(card, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_label_set_text(title, "Last 7 days");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 8);

    lv_obj_t *area = lv_obj_create(card);
    lv_obj_remove_style_all(area);
    lv_obj_set_size(area, CONTENT_W - 24, BAR_MAX_H);
    lv_obj_align(area, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_set_scrollable(area, false);

    int32_t col_w = (CONTENT_W - 24) / CHART_DAYS;
    for (int c = 0; c < CHART_DAYS; c++) {
        s_bars[c] = lv_obj_create(area);
        lv_obj_remove_style_all(s_bars[c]);
        lv_obj_set_width(s_bars[c], BAR_W);
        lv_obj_set_style_bg_opa(s_bars[c], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_bars[c], 4, 0);
        lv_obj_align(s_bars[c], LV_ALIGN_BOTTOM_LEFT, c * col_w + (col_w - BAR_W) / 2, 0);

        s_bar_days[c] = ui_label(card, &lv_font_montserrat_14, c == CHART_DAYS - 1 ? lv_color_white() : UI_COLOR_DIM);
        lv_obj_set_width(s_bar_days[c], col_w);
        lv_obj_set_style_text_align(s_bar_days[c], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_bar_days[c], LV_ALIGN_TOP_LEFT, c * col_w, 34 + BAR_MAX_H + 6);
    }

    s_goal_line = lv_obj_create(area);
    lv_obj_remove_style_all(s_goal_line);
    lv_obj_set_size(s_goal_line, CONTENT_W - 24, 1);
    lv_obj_set_style_bg_color(s_goal_line, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_goal_line, LV_OPA_40, 0);
}

static void create_goal(void)
{
    lv_obj_t *card = add_card(92);
    lv_obj_t *title = ui_label(card, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_label_set_text(title, "Daily goal");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *minus = ui_round_button(card, LV_SYMBOL_MINUS, 44, lv_color_hex(0x484848));
    lv_obj_set_gesture_bubble(minus, true);
    lv_obj_align(minus, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_add_event_cb(minus, on_goal, LV_EVENT_CLICKED, (void *)(intptr_t)-GOAL_STEP);

    s_goal = ui_label(card, &lv_font_montserrat_20, lv_color_white());
    lv_obj_align(s_goal, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t *plus = ui_round_button(card, LV_SYMBOL_PLUS, 44, lv_color_hex(0x484848));
    lv_obj_set_gesture_bubble(plus, true);
    lv_obj_align(plus, LV_ALIGN_BOTTOM_RIGHT, 0, -10);
    lv_obj_add_event_cb(plus, on_goal, LV_EVENT_CLICKED, (void *)(intptr_t)GOAL_STEP);
}

void health_app_create(lv_obj_t *parent)
{
    s_page = parent;
    lv_obj_set_scrollable(s_page, true);
    lv_obj_set_scroll_dir(s_page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(s_page, 16, 0);
    lv_obj_set_style_pad_bottom(s_page, 24, 0);
    lv_obj_set_style_pad_row(s_page, 10, 0);
    lv_obj_set_flex_flow(s_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_ring();

    lv_obj_t *averages = add_card(72);
    s_week_avg = add_average_row(averages, "7-day avg", 12);
    s_month_avg = add_average_row(averages, "30-day avg", 40);

    create_chart();
    create_goal();

    lv_timer_create(on_timer, 1000, NULL);
    refresh();
}

void health_app_on_show(void)
{
    lv_obj_scroll_to_y(s_page, 0, LV_ANIM_OFF);
    refresh();
}

bool health_app_is_showing(void)
{
    return s_page && lv_screen_active() == lv_obj_get_screen(s_page);
}

// ---- Goal reached ----

#define CELEBRATE_MS 5000

static lv_obj_t *s_celebrate;
static lv_obj_t *s_celebrate_steps;
static lv_timer_t *s_celebrate_timer;

static void close_celebration(void)
{
    if (s_celebrate_timer) {
        lv_timer_delete(s_celebrate_timer);
        s_celebrate_timer = NULL;
    }
    if (lv_screen_active() == s_celebrate) {
        ui_close_overlay();
    }
}

static void on_celebrate_timer(lv_timer_t *t)
{
    (void)t;
    s_celebrate_timer = NULL;
    close_celebration();
}

static void on_celebrate_tap(lv_event_t *e)
{
    (void)e;
    close_celebration();
}

static void build_celebration(void)
{
    s_celebrate = ui_screen_create();
    lv_obj_set_clickable(s_celebrate, true);
    lv_obj_add_event_cb(s_celebrate, on_celebrate_tap, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ring = lv_arc_create(s_celebrate);
    lv_obj_set_size(ring, 120, 120);
    lv_obj_align(ring, LV_ALIGN_CENTER, 0, -36);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_angles(ring, 0, 360);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(ring, false);
    lv_obj_set_style_arc_width(ring, RING_W, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, RING_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring, COLOR_GOAL_MET, LV_PART_INDICATOR);

    lv_obj_t *check = ui_label(s_celebrate, &lv_font_montserrat_48, COLOR_GOAL_MET);
    lv_label_set_text(check, LV_SYMBOL_OK);
    lv_obj_align_to(check, ring, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *title = ui_label(s_celebrate, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(title, "Goal reached!");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 54);

    s_celebrate_steps = ui_label(s_celebrate, &lv_font_montserrat_16, UI_COLOR_DIM);
    lv_obj_align(s_celebrate_steps, LV_ALIGN_CENTER, 0, 82);
}

void health_show_goal_reached(void)
{
    if (!s_celebrate) {
        build_celebration();
    }
    char buf[16];
    format_steps(buf, sizeof(buf), steps_goal());
    lv_label_set_text_fmt(s_celebrate_steps, "%s steps today", buf);
    lv_obj_align(s_celebrate_steps, LV_ALIGN_CENTER, 0, 82);

    if (s_celebrate_timer) {
        lv_timer_delete(s_celebrate_timer);
    }
    s_celebrate_timer = lv_timer_create(on_celebrate_timer, CELEBRATE_MS, NULL);
    lv_timer_set_repeat_count(s_celebrate_timer, 1);
    ui_show_overlay(s_celebrate);
}
