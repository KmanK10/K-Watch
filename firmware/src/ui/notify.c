#include "notify.h"

#include <string.h>

#include "lvgl.h"
#include "ui/text.h"
#include "ui/watchface.h"

#define COLOR_ACCENT lv_color_hex(0xFF5F1F)
#define CARD_WIDTH   200

static lv_obj_t *s_screen;
static lv_obj_t *s_app;
static lv_obj_t *s_title;
static lv_obj_t *s_message;
static uint32_t s_uid;

typedef struct {
    const char *bundle_id;
    const char *name;
} app_name_t;

static const app_name_t s_app_names[] = {
    {"com.apple.MobileSMS", "Messages"},
    {"com.apple.mobilephone", "Phone"},
    {"com.apple.mobilemail", "Mail"},
    {"com.apple.mobilecal", "Calendar"},
    {"com.apple.reminders", "Reminders"},
    {"com.apple.facetime", "FaceTime"},
    {"com.facebook.Messenger", "Messenger"},
    {"net.whatsapp.WhatsApp", "WhatsApp"},
    {"com.hammerandchisel.discord", "Discord"},
    {"com.toyopagroup.picaboo", "Snapchat"},
    {"com.burbn.instagram", "Instagram"},
    {"com.google.Gmail", "Gmail"},
};

// Falls back to the last part of the bundle ID, e.g. "com.example.Foo" -> "Foo".
static const char *app_display_name(const char *bundle_id)
{
    for (size_t i = 0; i < sizeof(s_app_names) / sizeof(s_app_names[0]); i++) {
        if (strcmp(bundle_id, s_app_names[i].bundle_id) == 0) {
            return s_app_names[i].name;
        }
    }
    const char *dot = strrchr(bundle_id, '.');
    return dot ? dot + 1 : bundle_id;
}

static void on_tap(lv_event_t *e)
{
    (void)e;
    watchface_show();
}

static lv_obj_t *make_label(const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(s_screen);
    lv_obj_set_width(label, CARD_WIDTH);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

static void create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_pad_top(s_screen, 28, 0);
    lv_obj_set_style_pad_bottom(s_screen, 28, 0);
    lv_obj_set_style_pad_row(s_screen, 8, 0);
    lv_obj_set_flex_flow(s_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(s_screen, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_screen, on_tap, LV_EVENT_SHORT_CLICKED, NULL);

    s_app = make_label(&lv_font_montserrat_16, COLOR_ACCENT);
    s_title = make_label(&lv_font_montserrat_20, lv_color_white());
    s_message = make_label(&lv_font_montserrat_16, lv_color_white());
}

void notify_show(const phone_notification_t *n)
{
    if (!s_screen) {
        create();
    }
    s_uid = n->uid;

    char buf[sizeof(n->message)];
    if (n->category == PHONE_CAT_INCOMING_CALL) {
        lv_label_set_text(s_app, LV_SYMBOL_CALL " Incoming call");
    } else {
        text_to_ascii(buf, sizeof(buf), app_display_name(n->app_id));
        lv_label_set_text(s_app, buf);
    }
    text_to_ascii(buf, sizeof(buf), n->title);
    lv_label_set_text(s_title, buf);
    text_to_ascii(buf, sizeof(buf), n->message);
    lv_label_set_text(s_message, buf);

    lv_obj_scroll_to_y(s_screen, 0, LV_ANIM_OFF);
    lv_screen_load(s_screen);
}

bool notify_is_showing(void)
{
    return s_screen && lv_screen_active() == s_screen;
}

uint32_t notify_current_uid(void)
{
    return s_uid;
}
