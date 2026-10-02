// K-Watch UI simulator: runs the watch's real ui/ code in a Windows window, with a
// fake phone. Drag with the mouse to swipe, and use the keys printed at startup
// to fake phone events.
//
//   k-watch-sim.exe              opens the window
//   k-watch-sim.exe --shots DIR  saves a PNG of every screen into DIR and exits

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <windowsx.h>

#include "lvgl.h"
#include "png.h"
#include "sim.h"
#include "ui/alert.h"
#include "ui/countdown.h"
#include "ui/music.h"
#include "ui/notify.h"
#include "ui/pairing.h"
#include "settings.h"
#include "ui/flashlight.h"
#include "ui/screens.h"
#include "ui/settings_screen.h"
#include "ui/watchface.h"

#define W            240
#define H            240
#define ZOOM         2
#define BUF_LINES    40

// ---- Clock ----

static bool s_fake_clock;
static int64_t s_fake_us;

int64_t esp_timer_get_time(void)
{
    if (s_fake_clock) {
        return s_fake_us;
    }
    static LARGE_INTEGER freq, start;
    LARGE_INTEGER now;
    if (!freq.QuadPart) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
    }
    QueryPerformanceCounter(&now);
    return (now.QuadPart - start.QuadPart) * 1000000 / freq.QuadPart;
}

static uint32_t tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// ---- Display and touch ----

static uint16_t s_fb[W * H];
static HWND s_hwnd;
static struct {
    int32_t x, y;
    bool pressed;
} s_touch;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    int32_t w = lv_area_get_width(area);
    const uint16_t *src = (const uint16_t *)px;
    for (int32_t y = area->y1; y <= area->y2; y++) {
        memcpy(&s_fb[y * W + area->x1], src, w * sizeof(uint16_t));
        src += w;
    }
    if (s_hwnd && lv_display_flush_is_last(disp)) {
        InvalidateRect(s_hwnd, NULL, FALSE);
    }
    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = s_touch.x;
    data->point.y = s_touch.y;
    data->state = s_touch.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void display_init(void)
{
    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_display_create(W, H);
    static uint16_t buf1[W * BUF_LINES], buf2[W * BUF_LINES];
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}

// ---- Fake phone ----

static phone_media_t s_media = {.playing = true, .volume = 60,
                                .title = "Blinding Lights", .artist = "The Weeknd"};
static bool s_connected = true;
static uint32_t s_next_uid = 100;

static const struct {
    const char *app_name, *title, *subtitle, *message;
} SAMPLES[] = {
    {"Messages", "Mom", "", "Are you coming for dinner on Sunday? Dad is making lasagna."},
    {"Gmail", "GitHub", "[K-Watch] New pull request", "TurtleWhal opened a pull request: round screen layout"},
    {"OfferUp", "OfferUp", "", "Price drop! The desk you saved is now $40."},
    {"Discord", "Garrett", "#watch-dev", "the new board just came in, want to see it?"},
    {"Strava", "Strava", "", "You just set a new personal record on Hill Climb!"},
};
#define SAMPLE_COUNT (sizeof(SAMPLES) / sizeof(SAMPLES[0]))

static phone_notification_t make_notification(size_t i, bool pre_existing)
{
    phone_notification_t n = {0};
    n.uid = s_next_uid++;
    n.category = PHONE_CAT_SOCIAL;
    n.pre_existing = pre_existing;
    n.has_negative = true;
    snprintf(n.app_name, sizeof(n.app_name), "%s", SAMPLES[i].app_name);
    snprintf(n.title, sizeof(n.title), "%s", SAMPLES[i].title);
    snprintf(n.subtitle, sizeof(n.subtitle), "%s", SAMPLES[i].subtitle);
    snprintf(n.message, sizeof(n.message), "%s", SAMPLES[i].message);
    return n;
}

static phone_notification_t make_call(void)
{
    phone_notification_t n = {0};
    n.uid = s_next_uid++;
    n.category = PHONE_CAT_INCOMING_CALL;
    n.has_positive = n.has_negative = true;
    snprintf(n.app_name, sizeof(n.app_name), "Phone");
    snprintf(n.title, sizeof(n.title), "Garrett");
    snprintf(n.message, sizeof(n.message), "Incoming Call");
    return n;
}

void sim_on_media_command(phone_media_cmd_t cmd)
{
    static const char *const TRACKS[][2] = {
        {"Blinding Lights", "The Weeknd"},
        {"Mr. Brightside", "The Killers"},
        {"A Really Long Song Title That Will Not Fit On The Screen", "Somebody"},
    };
    static int track;
    switch (cmd) {
    case PHONE_MEDIA_TOGGLE: s_media.playing = !s_media.playing; break;
    case PHONE_MEDIA_PLAY: s_media.playing = true; break;
    case PHONE_MEDIA_PAUSE: s_media.playing = false; break;
    case PHONE_MEDIA_VOLUME_UP: s_media.volume = s_media.volume >= 90 ? 100 : s_media.volume + 10; break;
    case PHONE_MEDIA_VOLUME_DOWN: s_media.volume = s_media.volume <= 10 ? 0 : s_media.volume - 10; break;
    case PHONE_MEDIA_NEXT:
    case PHONE_MEDIA_PREVIOUS:
        track = (track + (cmd == PHONE_MEDIA_NEXT ? 1 : 2)) % 3;
        snprintf(s_media.title, sizeof(s_media.title), "%s", TRACKS[track][0]);
        snprintf(s_media.artist, sizeof(s_media.artist), "%s", TRACKS[track][1]);
        break;
    }
    music_update(&s_media);
}

// ---- The watch, roughly as main.c drives it ----

static void refresh_watchface(void)
{
    struct tm t;
    if (s_fake_clock) {
        t = (struct tm){.tm_year = 126, .tm_mon = 9, .tm_mday = 2, .tm_wday = 5, .tm_hour = 10, .tm_min = 9};
    } else {
        time_t now = time(NULL);
        localtime_s(&t, &now);
    }
    watchface_set_time(&t);
    watchface_set_power(76, false, false);
    watchface_set_steps(4321);
    watchface_set_connected(s_connected);
}

static void apply_settings(const settings_t *s)
{
    printf("settings: brightness %d%%, timeout %d s, raise %d, tap %d, buzz %d, 24h %d, bluetooth %d, dnd %d\n",
           s->brightness, s->screen_timeout_s, s->raise_to_wake, s->tap_to_wake, s->notify_vibrate,
           s->clock_24h, s->bluetooth, s->dnd);
    watchface_set_24h(s->clock_24h);
    watchface_set_dnd(s->dnd);
    refresh_watchface();
}

static void on_flashlight(uint8_t percent)
{
    if (percent) {
        printf("flashlight on at %d%%\n", percent);
    } else {
        printf("flashlight off\n");
    }
}

static void forget_phone(void)
{
    phone_forget();
}

static void app_init(void)
{
    settings_init();
    ui_init();
    settings_on_change(apply_settings);
    settings_screen_on_forget(forget_phone);
    flashlight_on_change(on_flashlight);
    settings_screen_set_about("K-Watch simulator");
    refresh_watchface();
    music_set_connected(true);
    music_update(&s_media);
    for (size_t i = 0; i < SAMPLE_COUNT; i++) {
        phone_notification_t n = make_notification(i, true);
        notify_add(&n);
    }
}

static void show_new_notification(const phone_notification_t *n)
{
    notify_add(n);
    if (settings_get()->dnd) {
        printf("do not disturb: notification added to the list quietly\n");
        return;
    }
    notify_show_card(n->uid);
}

static void app_tick(void)
{
    static int64_t last_refresh;
    if (esp_timer_get_time() - last_refresh > 1000000) {
        last_refresh = esp_timer_get_time();
        refresh_watchface();
    }
    if (countdown_check_done()) {
        alert_show("Time's up", "Timer finished", NULL);
    }
}

static void on_key(int key)
{
    static size_t next_sample;
    switch (key) {
    case 'N': {
        phone_notification_t n = make_notification(next_sample++ % SAMPLE_COUNT, false);
        show_new_notification(&n);
        printf("[buzz] notify\n");
        break;
    }
    case 'S': {
        phone_notification_t n = make_notification(next_sample++ % SAMPLE_COUNT, false);
        n.silent = true;
        notify_add(&n);
        printf("silent notification added to the list\n");
        break;
    }
    case 'C': {
        phone_notification_t n = make_call();
        show_new_notification(&n);
        printf("[buzz] alert\n");
        break;
    }
    case 'R':
        if (s_next_uid > 100) {
            notify_remove(--s_next_uid);
            printf("notification %u removed on the phone\n", (unsigned)s_next_uid);
        }
        break;
    case 'P': pairing_show(123456); break;
    case 'A': alert_show("Time's up", "Timer finished", NULL); break;
    case 'D':
        s_connected = !s_connected;
        music_set_connected(s_connected);
        if (s_connected) {
            music_update(&s_media);
        }
        refresh_watchface();
        printf("phone %s\n", s_connected ? "connected" : "disconnected");
        break;
    case 'H':
        alert_dismiss();
        ui_show_home(true);
        break;
    case VK_ESCAPE:
    case 'Q':
        PostQuitMessage(0);
        break;
    }
}

// ---- Window ----

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        struct {
            BITMAPINFOHEADER h;
            DWORD masks[3];
        } bmi = {0};
        bmi.h.biSize = sizeof(BITMAPINFOHEADER);
        bmi.h.biWidth = W;
        bmi.h.biHeight = -H;
        bmi.h.biPlanes = 1;
        bmi.h.biBitCount = 16;
        bmi.h.biCompression = BI_BITFIELDS;
        bmi.masks[0] = 0xF800;
        bmi.masks[1] = 0x07E0;
        bmi.masks[2] = 0x001F;
        StretchDIBits(dc, 0, 0, W * ZOOM, H * ZOOM, 0, 0, W, H, s_fb, (BITMAPINFO *)&bmi, DIB_RGB_COLORS,
                      SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        s_touch.pressed = true;
        // fall through
    case WM_MOUSEMOVE:
        if (s_touch.pressed) {
            s_touch.x = GET_X_LPARAM(lp) / ZOOM;
            s_touch.y = GET_Y_LPARAM(lp) / ZOOM;
        }
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        s_touch.pressed = false;
        return 0;
    case WM_KEYDOWN:
        on_key((int)wp);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int run_window(void)
{
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.lpszClassName = L"KWatchSim";
    RegisterClassW(&wc);

    RECT r = {0, 0, W * ZOOM, H * ZOOM};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    s_hwnd = CreateWindowW(wc.lpszClassName, L"K-Watch simulator", style, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
    ShowWindow(s_hwnd, SW_SHOW);

    printf("K-Watch simulator. Drag with the mouse to swipe. Keys:\n"
           "  N  new notification      S  silent notification   C  incoming call\n"
           "  R  remove newest (dismissed on phone)              D  phone connect/disconnect\n"
           "  P  pairing code          A  ringing alert         H  home      Q  quit\n");

    timeBeginPeriod(1);
    MSG msg;
    while (true) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                timeEndPeriod(1);
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        app_tick();
        uint32_t next = lv_timer_handler();
        Sleep(next < 1 ? 1 : (next > 10 ? 10 : next));
    }
}

// ---- Screenshots ----

static void advance(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 5) {
        s_fake_us += 5000;
        app_tick();
        lv_timer_handler();
    }
}

static bool on_screen(lv_obj_t *obj)
{
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    return a.x1 >= 0 && a.y1 >= 0 && a.x2 < W && a.y2 < H && !lv_obj_is_hidden(obj);
}

// The visible button whose label reads `text`.
static lv_obj_t *find_button(lv_obj_t *root, const char *text)
{
    uint32_t n = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(root, i);
        if (lv_obj_check_type(child, &lv_label_class) && strcmp(lv_label_get_text(child), text) == 0 &&
            on_screen(root)) {
            return root;
        }
        lv_obj_t *found = find_button(child, text);
        if (found) {
            return found;
        }
    }
    return NULL;
}

static void tap_at(int32_t x, int32_t y)
{
    s_touch.x = x;
    s_touch.y = y;
    s_touch.pressed = true;
    advance(80);
    s_touch.pressed = false;
    advance(80);
}

static void tap(const char *label)
{
    lv_obj_t *btn = find_button(lv_screen_active(), label);
    if (!btn) {
        printf("no visible button labelled %s\n", label);
        return;
    }
    lv_area_t a;
    lv_obj_get_coords(btn, &a);
    s_touch.x = (a.x1 + a.x2) / 2;
    s_touch.y = (a.y1 + a.y2) / 2;
    s_touch.pressed = true;
    advance(80);
    s_touch.pressed = false;
    advance(80);
}

static void shot(const char *dir, const char *name)
{
    advance(600);
    lv_refr_now(NULL);
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s.png", dir, name);
    if (png_write_rgb565(path, s_fb, W, H)) {
        printf("saved %s\n", path);
    } else {
        printf("could not write %s\n", path);
    }
}

static int run_shots(const char *dir)
{
    CreateDirectoryA(dir, NULL);

    shot(dir, "01-watchface");
    ui_show_screen("music", false);
    shot(dir, "02-music");
    tap(LV_SYMBOL_NEXT);
    tap(LV_SYMBOL_NEXT);
    shot(dir, "02-music-long-title");
    ui_show_screen("notifications", false);
    shot(dir, "03-notifications");
    notify_show_card(100);
    shot(dir, "04-notification-card");
    phone_notification_t call = make_call();
    show_new_notification(&call);
    shot(dir, "05-incoming-call");

    ui_show_screen("apps", false);
    shot(dir, "14-apps");
    tap(LV_SYMBOL_BELL);
    shot(dir, "06-timer");
    tap(LV_SYMBOL_PLAY);
    advance(83 * 1000);
    shot(dir, "07-timer-running");
    tap(LV_SYMBOL_PAUSE);
    ui_close_overlay();
    advance(600);

    tap(LV_SYMBOL_LOOP);
    advance(600);
    tap(LV_SYMBOL_PLAY);
    advance(61 * 1000);
    tap(LV_SYMBOL_LOOP);
    advance(58 * 1000);
    tap(LV_SYMBOL_LOOP);
    advance(63 * 1000);
    tap(LV_SYMBOL_LOOP);
    advance(12 * 1000);
    shot(dir, "08-stopwatch");
    ui_close_overlay();
    advance(600);

    ui_show_screen("settings", false);
    shot(dir, "11-settings");
    lv_obj_t *page = lv_obj_get_parent(find_button(lv_screen_active(), "Brightness"));
    lv_obj_scroll_to_y(page, LV_COORD_MAX, LV_ANIM_OFF);
    shot(dir, "12-settings-bottom");
    lv_obj_scroll_to_y(page, 0, LV_ANIM_OFF);
    tap("Do not disturb");
    ui_show_home(false);
    shot(dir, "13-watchface-dnd");
    ui_show_screen("settings", false);
    tap("Do not disturb");

    ui_show_screen("apps", false);
    tap(LV_SYMBOL_CHARGE);
    shot(dir, "15-flashlight");
    tap_at(120, 60);    // the light: show the controls
    shot(dir, "16-flashlight-controls");
    tap_at(120, 181);   // the red swatch
    tap_at(120, 60);    // the light: hide the controls
    shot(dir, "17-flashlight-red");
    ui_close_overlay();
    advance(600);

    alert_show("Time's up", "Timer finished", NULL);
    shot(dir, "09-alert");
    alert_dismiss();
    pairing_show(123456);
    shot(dir, "10-pairing");
    return 0;
}

int main(int argc, char **argv)
{
    bool shots = argc >= 3 && strcmp(argv[1], "--shots") == 0;
    s_fake_clock = shots;
    display_init();
    app_init();
    return shots ? run_shots(argv[2]) : run_window();
}
