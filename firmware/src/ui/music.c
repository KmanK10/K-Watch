#include "music.h"

#include "lvgl.h"
#include "ui/text.h"
#include "ui/theme.h"

#define CONTENT_W 190

static lv_obj_t *s_title;
static lv_obj_t *s_artist;
static lv_obj_t *s_play_label;
static lv_obj_t *s_volume;

static phone_media_t s_media;
static bool s_connected;

static void on_command(lv_event_t *e)
{
    phone_media_command((phone_media_cmd_t)(uintptr_t)lv_event_get_user_data(e));
}

static lv_obj_t *add_button(lv_obj_t *parent, const char *symbol, int32_t size, lv_color_t bg,
                            phone_media_cmd_t cmd)
{
    lv_obj_t *btn = ui_round_button(parent, symbol, size, bg);
    lv_obj_add_event_cb(btn, on_command, LV_EVENT_CLICKED, (void *)(uintptr_t)cmd);
    return btn;
}

static lv_obj_t *row(lv_obj_t *parent, int32_t y)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, CONTENT_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(r, LV_ALIGN_CENTER, 0, y);
    return r;
}

static void refresh(void)
{
    if (!s_title) {
        return;
    }
    char buf[sizeof(s_media.title)];
    if (!s_connected) {
        lv_label_set_text(s_title, "Phone not connected");
        lv_label_set_text(s_artist, "");
    } else if (s_media.title[0] == '\0') {
        lv_label_set_text(s_title, "Nothing playing");
        lv_label_set_text(s_artist, "");
    } else {
        text_to_ascii(buf, sizeof(buf), s_media.title);
        lv_label_set_text(s_title, buf);
        text_to_ascii(buf, sizeof(buf), s_media.artist);
        lv_label_set_text(s_artist, buf);
    }
    lv_label_set_text(s_play_label, s_media.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_label_set_text_fmt(s_volume, LV_SYMBOL_VOLUME_MAX " %d%%", s_media.volume);
}

void music_create(lv_obj_t *parent)
{
    s_title = ui_label(parent, &lv_font_montserrat_20, lv_color_white());
    lv_obj_set_width(s_title, CONTENT_W);
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(s_title, LV_ALIGN_CENTER, 0, -62);

    s_artist = ui_label(parent, &lv_font_montserrat_16, UI_COLOR_ACCENT);
    lv_obj_set_width(s_artist, CONTENT_W);
    lv_obj_set_style_text_align(s_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_artist, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(s_artist, LV_ALIGN_CENTER, 0, -36);

    lv_obj_t *controls = row(parent, 10);
    add_button(controls, LV_SYMBOL_PREV, 48, UI_COLOR_BUTTON, PHONE_MEDIA_PREVIOUS);
    lv_obj_t *play = add_button(controls, LV_SYMBOL_PLAY, 64, UI_COLOR_ACCENT, PHONE_MEDIA_TOGGLE);
    s_play_label = lv_obj_get_child(play, 0);
    add_button(controls, LV_SYMBOL_NEXT, 48, UI_COLOR_BUTTON, PHONE_MEDIA_NEXT);

    lv_obj_t *vol = row(parent, 72);
    add_button(vol, LV_SYMBOL_MINUS, 40, UI_COLOR_BUTTON, PHONE_MEDIA_VOLUME_DOWN);
    s_volume = ui_label(vol, &lv_font_montserrat_16, UI_COLOR_DIM);
    add_button(vol, LV_SYMBOL_PLUS, 40, UI_COLOR_BUTTON, PHONE_MEDIA_VOLUME_UP);

    refresh();
}

void music_update(const phone_media_t *media)
{
    s_media = *media;
    refresh();
}

void music_set_connected(bool connected)
{
    s_connected = connected;
    if (!connected) {
        s_media = (phone_media_t){0};
    }
    refresh();
}
