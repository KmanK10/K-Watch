#include "screens.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "ui/music.h"
#include "ui/notify.h"
#include "ui/theme.h"
#include "ui/watchface.h"

#define OVERLAY_ANIM_MS 200

typedef struct {
    const char *name;
    int8_t col;                          // position relative to the watch face at (0, 0)
    int8_t row;
    void (*create)(lv_obj_t *parent);    // builds the screen's content into `parent`
    void (*on_show)(void);               // optional, called each time it is swiped to
} ui_screen_t;

// Swipe left on the watch face for music, swipe up for notifications.
// The home screen must stay first.
static const ui_screen_t s_screens[] = {
    {"watch face", 0, 0, watchface_create, NULL},
    {"music", 1, 0, music_create, NULL},
    {"notifications", 0, 1, notify_list_create, notify_list_on_show},
};
#define SCREEN_COUNT (sizeof(s_screens) / sizeof(s_screens[0]))
#define HOME 0

static const char *TAG = "screens";

static lv_obj_t *s_main;
static lv_obj_t *s_tileview;
static lv_obj_t *s_tiles[SCREEN_COUNT];

// Swiping right always goes home. A screen with nothing to its left gets a stand-in
// tile there showing a picture of the watch face, so the swipe follows the finger
// like any other; landing on it jumps to the real watch face.
static lv_obj_t *s_home_ghosts[SCREEN_COUNT];
static lv_obj_t *s_home_ghost_images[SCREEN_COUNT];
static lv_draw_buf_t s_home_snapshot;
static bool s_home_snapshot_ready;

static bool has_screen_at(int col, int row)
{
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (s_screens[i].col == col && s_screens[i].row == row) {
            return true;
        }
    }
    return false;
}

static bool needs_home_ghost(size_t i)
{
    return i != HOME && !has_screen_at(s_screens[i].col - 1, s_screens[i].row);
}

static void alloc_home_snapshot(void)
{
    lv_obj_update_layout(s_tileview);
    uint32_t w = lv_obj_get_width(s_tileview);
    uint32_t h = lv_obj_get_height(s_tileview);
    uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
    uint32_t size = stride * h;
    void *data = heap_caps_aligned_alloc(64, size, MALLOC_CAP_SPIRAM);
    if (data == NULL) {
        ESP_LOGW(TAG, "no memory for the home snapshot, swipe right will jump");
        return;
    }
    lv_draw_buf_init(&s_home_snapshot, w, h, LV_COLOR_FORMAT_RGB565, stride, data, size);
    s_home_snapshot_ready = true;
}

static void refresh_home_snapshot(void)
{
    if (!s_home_snapshot_ready ||
        lv_snapshot_take_to_draw_buf(s_tiles[HOME], LV_COLOR_FORMAT_RGB565, &s_home_snapshot) != LV_RESULT_OK) {
        return;
    }
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (s_home_ghost_images[i]) {
            lv_image_set_src(s_home_ghost_images[i], &s_home_snapshot);
            lv_obj_invalidate(s_home_ghost_images[i]);
        }
    }
}

static bool is_home_ghost(lv_obj_t *tile)
{
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (s_home_ghosts[i] && s_home_ghosts[i] == tile) {
            return true;
        }
    }
    return false;
}

static bool finger_down(void)
{
    lv_indev_t *indev = lv_indev_active();
    return indev && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;
}

// While a finger drags, the tile view only lets it move in directions the starting tile
// allows, so reversing a swipe gets stuck. Track the drag and also allow the way back.
static bool s_dragging;
static lv_point_t s_drag_start;
static lv_dir_t s_drag_dir;

static void on_scroll_begin(lv_event_t *e)
{
    (void)e;
    lv_obj_t *active = lv_tileview_get_tile_active(s_tileview);
    s_dragging = finger_down();
    if (s_dragging) {
        s_drag_start.x = lv_obj_get_x(active);
        s_drag_start.y = lv_obj_get_y(active);
        s_drag_dir = lv_obj_get_scroll_dir(s_tileview);
    }

    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (s_tiles[i] == active && s_home_ghosts[i]) {
            refresh_home_snapshot();
        }
    }
}

static void on_scroll(lv_event_t *e)
{
    (void)e;
    if (!s_dragging || !finger_down()) {
        return;
    }
    int32_t dx = lv_obj_get_scroll_x(s_tileview) - s_drag_start.x;
    int32_t dy = lv_obj_get_scroll_y(s_tileview) - s_drag_start.y;
    lv_dir_t dir = s_drag_dir;
    if (dx < 0) dir |= LV_DIR_RIGHT;
    if (dx > 0) dir |= LV_DIR_LEFT;
    if (dy < 0) dir |= LV_DIR_BOTTOM;
    if (dy > 0) dir |= LV_DIR_TOP;
    lv_obj_set_scroll_dir(s_tileview, dir);
}

static void jump_home(void *arg)
{
    (void)arg;
    lv_tileview_set_tile(s_tileview, s_tiles[HOME], LV_ANIM_OFF);
}

static void on_scroll_end(lv_event_t *e)
{
    (void)e;
    if (finger_down()) {
        return;
    }
    s_dragging = false;

    // The tile view picks the new tile as soon as the settle animation starts; only
    // swap the stand-in for the real watch face once it has fully slid into place.
    lv_obj_t *active = lv_tileview_get_tile_active(s_tileview);
    if (is_home_ghost(active) &&
        lv_obj_get_scroll_x(s_tileview) == lv_obj_get_x(active) &&
        lv_obj_get_scroll_y(s_tileview) == lv_obj_get_y(active)) {
        // The tile view finishes its own bookkeeping after this event; move on after.
        lv_async_call(jump_home, NULL);
    }
}

static void on_tile_changed(lv_event_t *e)
{
    (void)e;
    lv_obj_t *active = lv_tileview_get_tile_active(s_tileview);
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (s_tiles[i] == active && s_screens[i].on_show) {
            s_screens[i].on_show();
        }
    }
}

void ui_init(void)
{
    s_main = lv_screen_active();
    lv_obj_set_style_bg_color(s_main, lv_color_black(), 0);
    lv_obj_set_scrollable(s_main, false);

    s_tileview = lv_tileview_create(s_main);
    lv_obj_set_size(s_tileview, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_tileview, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);

    // The tile view only takes non-negative positions.
    int min_col = 0, min_row = 0;
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        int col = needs_home_ghost(i) ? s_screens[i].col - 1 : s_screens[i].col;
        min_col = col < min_col ? col : min_col;
        min_row = s_screens[i].row < min_row ? s_screens[i].row : min_row;
    }

    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        const ui_screen_t *s = &s_screens[i];
        lv_dir_t dir = LV_DIR_NONE;
        if (has_screen_at(s->col - 1, s->row) || needs_home_ghost(i)) dir |= LV_DIR_LEFT;
        if (has_screen_at(s->col + 1, s->row)) dir |= LV_DIR_RIGHT;
        if (has_screen_at(s->col, s->row - 1)) dir |= LV_DIR_TOP;
        if (has_screen_at(s->col, s->row + 1)) dir |= LV_DIR_BOTTOM;

        s_tiles[i] = lv_tileview_add_tile(s_tileview, s->col - min_col, s->row - min_row, dir);
        lv_obj_set_scrollbar_mode(s_tiles[i], LV_SCROLLBAR_MODE_OFF);
        s->create(s_tiles[i]);

        if (needs_home_ghost(i)) {
            s_home_ghosts[i] = lv_tileview_add_tile(s_tileview, s->col - 1 - min_col, s->row - min_row,
                                                    LV_DIR_RIGHT);
            lv_obj_set_scrollable(s_home_ghosts[i], false);
            s_home_ghost_images[i] = lv_image_create(s_home_ghosts[i]);
        }
    }

    lv_tileview_set_tile(s_tileview, s_tiles[HOME], LV_ANIM_OFF);
    alloc_home_snapshot();
    lv_obj_add_event_cb(s_tileview, on_tile_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_tileview, on_scroll_begin, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(s_tileview, on_scroll, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(s_tileview, on_scroll_end, LV_EVENT_SCROLL_END, NULL);
}

void ui_show_home(bool animate)
{
    if (lv_screen_active() != s_main) {
        lv_screen_load(s_main);
    }
    lv_tileview_set_tile(s_tileview, s_tiles[HOME], animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

void ui_show_overlay(lv_obj_t *screen)
{
    if (lv_screen_active() == screen) {
        return;
    }
    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, OVERLAY_ANIM_MS, 0, false);
}

void ui_close_overlay(void)
{
    if (lv_screen_active() == s_main) {
        return;
    }
    lv_screen_load_anim(s_main, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, OVERLAY_ANIM_MS, 0, false);
}
