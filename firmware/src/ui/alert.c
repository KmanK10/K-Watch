#include "alert.h"

#include "haptics.h"
#include "lvgl.h"
#include "ui/screens.h"
#include "ui/theme.h"

#define BUZZ_EVERY_MS 2500

static lv_obj_t *s_screen;
static lv_obj_t *s_title;
static lv_obj_t *s_detail;
static lv_obj_t *s_stop;
static lv_obj_t *s_snooze;
static lv_timer_t *s_buzz;
static void (*s_on_stop)(void);
static void (*s_on_snooze)(void);

static void buzz(lv_timer_t *t)
{
    (void)t;
    haptics_play(HAPTIC_ALERT);
}

static void stop_buzzing(void)
{
    if (s_buzz) {
        lv_timer_delete(s_buzz);
        s_buzz = NULL;
    }
}

static void on_button(lv_event_t *e)
{
    bool snooze = lv_event_get_target(e) == s_snooze;
    void (*cb)(void) = snooze ? s_on_snooze : s_on_stop;
    alert_dismiss();
    if (cb) {
        cb();
    }
}

static void build(void)
{
    s_screen = ui_screen_create();

    lv_obj_t *icon = ui_label(s_screen, &lv_font_montserrat_48, UI_COLOR_ACCENT);
    lv_label_set_text(icon, LV_SYMBOL_BELL);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -70);

    s_title = ui_label(s_screen, &lv_font_montserrat_20, lv_color_white());
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, -14);

    s_detail = ui_label(s_screen, &lv_font_montserrat_16, UI_COLOR_DIM);
    lv_obj_align(s_detail, LV_ALIGN_CENTER, 0, 14);

    s_stop = ui_round_button(s_screen, "Stop", 72, UI_COLOR_ACCENT);
    lv_obj_add_event_cb(s_stop, on_button, LV_EVENT_CLICKED, NULL);

    s_snooze = ui_round_button(s_screen, "Snooze", 72, UI_COLOR_BUTTON);
    lv_obj_set_width(s_snooze, 104);
    lv_obj_align(s_snooze, LV_ALIGN_CENTER, -56, 74);
    lv_obj_add_event_cb(s_snooze, on_button, LV_EVENT_CLICKED, NULL);
}

void alert_show(const char *title, const char *detail, void (*on_stop_cb)(void), void (*on_snooze_cb)(void))
{
    if (!s_screen) {
        build();
    }
    lv_label_set_text(s_title, title);
    lv_label_set_text(s_detail, detail ? detail : "");
    s_on_stop = on_stop_cb;
    s_on_snooze = on_snooze_cb;

    lv_obj_set_hidden(s_snooze, on_snooze_cb == NULL);
    if (on_snooze_cb) {
        lv_obj_set_width(s_stop, 104);
        lv_obj_align(s_stop, LV_ALIGN_CENTER, 56, 74);
    } else {
        lv_obj_set_width(s_stop, 140);
        lv_obj_align(s_stop, LV_ALIGN_CENTER, 0, 74);
    }

    ui_show_overlay(s_screen);
    if (!s_buzz) {
        buzz(NULL);
        s_buzz = lv_timer_create(buzz, BUZZ_EVERY_MS, NULL);
    }
}

void alert_dismiss(void)
{
    stop_buzzing();
    s_on_stop = NULL;
    s_on_snooze = NULL;
    if (alert_is_showing()) {
        ui_close_overlay();
    }
}

bool alert_is_showing(void)
{
    return s_screen && lv_screen_active() == s_screen;
}
