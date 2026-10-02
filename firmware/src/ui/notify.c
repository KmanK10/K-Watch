#include "notify.h"

#include <string.h>
#include <strings.h>

#include "lvgl.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/watchface.h"

#define LIST_MAX     10
#define CONTENT_W    200

static phone_notification_t s_items[LIST_MAX];   // newest first
static int s_count;

static lv_obj_t *s_card;
static lv_obj_t *s_card_app;
static lv_obj_t *s_card_title;
static lv_obj_t *s_card_subtitle;
static lv_obj_t *s_card_message;
static lv_obj_t *s_card_actions;
static uint32_t s_card_uid;
static bool s_card_from_list;

static lv_obj_t *s_list;
static lv_obj_t *s_list_items;

// Falls back to the last part of the bundle ID, e.g. "com.example.Foo" -> "Foo".
static const char *app_display_name(const phone_notification_t *n)
{
    if (n->app_name[0]) {
        return n->app_name;
    }
    const char *dot = strrchr(n->app_id, '.');
    return dot ? dot + 1 : n->app_id;
}

// Apps without a title get their own name as the title, which would just repeat the header.
static bool title_is_useful(const phone_notification_t *n)
{
    return n->title[0] && strcasecmp(n->title, app_display_name(n)) != 0;
}

static void set_ascii_text(lv_obj_t *label, const char *utf8)
{
    char buf[sizeof(((phone_notification_t *)0)->message)];
    text_to_ascii(buf, sizeof(buf), utf8);
    lv_label_set_text(label, buf);
}

static int find(uint32_t uid)
{
    for (int i = 0; i < s_count; i++) {
        if (s_items[i].uid == uid) {
            return i;
        }
    }
    return -1;
}

static bool card_showing(void)
{
    return s_card && lv_screen_active() == s_card;
}

static bool list_showing(void)
{
    return s_list && lv_screen_active() == s_list;
}

// ---- Card ----

static void card_back(void)
{
    if (s_card_from_list) {
        notify_show_list();
    } else {
        watchface_show();
    }
}

static void on_card_back(lv_event_t *e)
{
    (void)e;
    card_back();
}

static void on_action(lv_event_t *e)
{
    bool positive = (bool)(uintptr_t)lv_event_get_user_data(e);
    phone_notification_action(s_card_uid, positive);
    card_back();
}

static lv_obj_t *card_label(const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = ui_label(s_card, font, color);
    lv_obj_set_width(label, CONTENT_W);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void create_card(void)
{
    s_card = ui_screen_create();
    lv_obj_set_style_pad_top(s_card, 28, 0);
    lv_obj_set_style_pad_bottom(s_card, 36, 0);
    lv_obj_set_style_pad_row(s_card, 8, 0);
    lv_obj_set_flex_flow(s_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(s_card, true);
    lv_obj_set_scroll_dir(s_card, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_card, on_card_back, LV_EVENT_SHORT_CLICKED, NULL);
    ui_on_swipe(s_card, LV_EVENT_GESTURE_RIGHT, card_back);

    s_card_app = card_label(&lv_font_montserrat_16, UI_COLOR_ACCENT);
    s_card_title = card_label(&lv_font_montserrat_20, lv_color_white());
    s_card_subtitle = card_label(&lv_font_montserrat_16, lv_color_white());
    s_card_message = card_label(&lv_font_montserrat_16, lv_color_white());

    s_card_actions = lv_obj_create(s_card);
    lv_obj_remove_style_all(s_card_actions);
    lv_obj_set_size(s_card_actions, CONTENT_W, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(s_card_actions, 8, 0);
    lv_obj_set_flex_flow(s_card_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_card_actions, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
}

static void add_action(const char *symbol, lv_color_t color, bool positive)
{
    lv_obj_t *btn = ui_round_button(s_card_actions, symbol, 52, color);
    lv_obj_add_event_cb(btn, on_action, LV_EVENT_CLICKED, (void *)(uintptr_t)positive);
}

void notify_show_card(uint32_t uid)
{
    int i = find(uid);
    if (i < 0) {
        return;
    }
    const phone_notification_t *n = &s_items[i];
    if (!s_card) {
        create_card();
    }
    s_card_from_list = list_showing() || (card_showing() && s_card_from_list);
    s_card_uid = uid;

    bool call = n->category == PHONE_CAT_INCOMING_CALL;
    if (call) {
        lv_label_set_text(s_card_app, LV_SYMBOL_CALL " Incoming call");
    } else {
        set_ascii_text(s_card_app, app_display_name(n));
    }
    set_ascii_text(s_card_title, n->title);
    lv_obj_set_hidden(s_card_title, !title_is_useful(n));
    set_ascii_text(s_card_subtitle, n->subtitle);
    lv_obj_set_hidden(s_card_subtitle, n->subtitle[0] == '\0');
    set_ascii_text(s_card_message, n->message);
    lv_obj_set_hidden(s_card_message, n->message[0] == '\0');

    lv_obj_clean(s_card_actions);
    if (call && n->has_negative) {
        add_action(LV_SYMBOL_CLOSE, lv_palette_main(LV_PALETTE_RED), false);
    }
    if (call && n->has_positive) {
        add_action(LV_SYMBOL_CALL, lv_palette_main(LV_PALETTE_GREEN), true);
    }
    if (!call && n->has_negative) {
        add_action(LV_SYMBOL_TRASH, UI_COLOR_BUTTON, false);
    }
    lv_obj_set_hidden(s_card_actions, lv_obj_get_child_count(s_card_actions) == 0);

    lv_obj_scroll_to_y(s_card, 0, LV_ANIM_OFF);
    lv_screen_load(s_card);
}

// ---- List ----

static void on_item_clicked(lv_event_t *e)
{
    notify_show_card((uint32_t)(uintptr_t)lv_event_get_user_data(e));
}

static void create_list(void)
{
    s_list = ui_screen_create();
    lv_obj_set_scrollable(s_list, true);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(s_list, 16, 0);
    lv_obj_set_style_pad_bottom(s_list, 40, 0);
    lv_obj_set_style_pad_row(s_list, 8, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_on_swipe(s_list, LV_EVENT_GESTURE_RIGHT, watchface_show);

    lv_obj_t *heading = ui_label(s_list, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_label_set_text(heading, LV_SYMBOL_BELL " Notifications");

    s_list_items = lv_obj_create(s_list);
    lv_obj_remove_style_all(s_list_items);
    lv_obj_set_size(s_list_items, CONTENT_W, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(s_list_items, 8, 0);
    lv_obj_set_flex_flow(s_list_items, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_gesture_bubble(s_list_items, true);
}

static lv_obj_t *item_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color,
                            const char *text)
{
    lv_obj_t *label = ui_label(parent, font, color);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    set_ascii_text(label, text);
    return label;
}

static void rebuild_list(void)
{
    lv_obj_clean(s_list_items);
    if (s_count == 0) {
        lv_obj_t *empty = ui_label(s_list_items, &lv_font_montserrat_16, UI_COLOR_DIM);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(empty, "Nothing new");
        return;
    }
    for (int i = 0; i < s_count; i++) {
        const phone_notification_t *n = &s_items[i];
        lv_obj_t *item = lv_obj_create(s_list_items);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(item, UI_COLOR_BUTTON, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(item, 12, 0);
        lv_obj_set_style_pad_all(item, 8, 0);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_clickable(item, true);
        lv_obj_set_gesture_bubble(item, true);
        lv_obj_add_event_cb(item, on_item_clicked, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)n->uid);

        item_label(item, &lv_font_montserrat_16, UI_COLOR_ACCENT, app_display_name(n));
        const char *heading = title_is_useful(n) ? n->title : n->subtitle;
        if (heading[0]) {
            item_label(item, &lv_font_montserrat_16, lv_color_white(), heading);
        }
        if (n->message[0]) {
            item_label(item, &lv_font_montserrat_16, UI_COLOR_DIM, n->message);
        }
    }
}

void notify_show_list(void)
{
    if (!s_list) {
        create_list();
    }
    rebuild_list();
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
    lv_screen_load(s_list);
}

// ---- Store ----

void notify_add(const phone_notification_t *n)
{
    int existing = find(n->uid);
    int shift_count = existing >= 0 ? existing : (s_count < LIST_MAX ? s_count : LIST_MAX - 1);
    memmove(&s_items[1], &s_items[0], shift_count * sizeof(s_items[0]));
    s_items[0] = *n;
    if (existing < 0 && s_count < LIST_MAX) {
        s_count++;
    }
    if (list_showing()) {
        rebuild_list();
    }
}

void notify_remove(uint32_t uid)
{
    int i = find(uid);
    if (i < 0) {
        return;
    }
    memmove(&s_items[i], &s_items[i + 1], (s_count - i - 1) * sizeof(s_items[0]));
    s_count--;

    if (card_showing() && s_card_uid == uid) {
        card_back();
    } else if (list_showing()) {
        rebuild_list();
    }
}

void notify_clear(void)
{
    s_count = 0;
    if (card_showing()) {
        card_back();
    } else if (list_showing()) {
        rebuild_list();
    }
}
