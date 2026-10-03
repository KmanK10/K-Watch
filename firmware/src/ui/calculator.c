#include "calculator.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "haptics.h"
#include "ui/theme.h"

#define DISPLAY_H      64
#define KEY_W          57
#define KEY_H          41
#define KEY_GAP        4
#define VALUE_W        176

#define MAX_NUMBERS    8
#define MAX_DIGITS     9
#define EXPR_MAX_CHARS 24

#define COLOR_DIGIT    lv_color_hex(0x333333)
#define COLOR_EQUALS   lv_color_hex(0x2EBD59)

// Row by row; the operators are drawn as shapes since the font has no × or ÷.
static const char KEYS[] = "789/456*123-.0=+";

static lv_obj_t *s_expr;
static lv_obj_t *s_value;
static lv_obj_t *s_back_label;
static lv_obj_t *s_op_keys[4];
static const char OPS[] = "/*-+";

// The expression is numbers[0] ops[0] numbers[1] ops[1] ... then whatever is being typed.
static double s_numbers[MAX_NUMBERS];
static char s_ops[MAX_NUMBERS];
static int s_count;
static char s_entry[MAX_DIGITS + 3];   // digits, maybe a "-" and a "."
static bool s_showing_result;
static bool s_error;

static void append(char *buf, size_t size, const char *text)
{
    size_t len = strlen(buf);
    snprintf(buf + len, size - len, "%s", text);
}

// ---- Maths ----

// × and ÷ first, then + and − from left to right. False on division by zero.
static bool evaluate(const double *numbers, const char *ops, int n, double *result)
{
    double sum = 0;
    char add_op = '+';
    double term = numbers[0];
    for (int i = 1; i < n; i++) {
        char op = ops[i - 1];
        if (op == '*') {
            term *= numbers[i];
        } else if (op == '/') {
            if (numbers[i] == 0) {
                return false;
            }
            term /= numbers[i];
        } else {
            sum = add_op == '+' ? sum + term : sum - term;
            add_op = op;
            term = numbers[i];
        }
    }
    *result = add_op == '+' ? sum + term : sum - term;
    return isfinite(*result);
}

static void format_number(char *buf, size_t size, double v)
{
    if (v == 0) {
        v = 0;   // no "-0"
    }
    double mag = fabs(v);
    if (mag != 0 && (mag >= 1e10 || mag < 1e-6)) {
        snprintf(buf, size, "%.4g", v);
    } else {
        snprintf(buf, size, "%.10g", v);
    }
}

// ---- Display ----

static const char *op_text(char op)
{
    switch (op) {
    case '*': return " x ";
    case '/': return " / ";
    case '-': return " - ";
    default: return " + ";
    }
}

static void build_expression(char *buf, size_t size, bool with_entry)
{
    char full[160] = "";
    char num[24];
    for (int i = 0; i < s_count; i++) {
        format_number(num, sizeof(num), s_numbers[i]);
        append(full, sizeof(full), num);
        append(full, sizeof(full), op_text(s_ops[i]));
    }
    if (with_entry) {
        append(full, sizeof(full), s_entry);
    }
    // Keep the end, which is what was just typed.
    size_t len = strlen(full);
    if (len > EXPR_MAX_CHARS) {
        snprintf(buf, size, "...%s", full + len - (EXPR_MAX_CHARS - 3));
    } else {
        snprintf(buf, size, "%s", full);
    }
}

static void set_value(const char *text)
{
    size_t len = strlen(text);
    const lv_font_t *font = len <= 6 ? &lv_font_montserrat_48
                          : len <= 9 ? &lv_font_montserrat_32
                                     : &lv_font_montserrat_20;
    lv_obj_set_style_text_font(s_value, font, 0);
    lv_label_set_text(s_value, text);
    // Sits on the bottom of the display area whatever the size. Digits have no descenders,
    // so the bottom eighth of the line can overlap the gap above the keys.
    int32_t line_h = lv_font_get_line_height(font);
    lv_obj_align(s_value, LV_ALIGN_TOP_RIGHT, -8, DISPLAY_H - line_h + line_h / 8);
}

static void set_glyph_color(lv_obj_t *obj, lv_color_t color)
{
    uint32_t n = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(obj, (int32_t)i);
        if (lv_obj_get_child_count(child)) {
            set_glyph_color(child, color);
        } else {
            lv_obj_set_style_bg_color(child, color, 0);
        }
    }
}

static void refresh(void)
{
    char buf[64];
    // The operator waiting for its next number stays lit.
    char pending = s_entry[0] == '\0' && s_count > 0 && !s_showing_result ? s_ops[s_count - 1] : 0;
    for (int i = 0; i < 4; i++) {
        bool lit = OPS[i] == pending;
        lv_obj_set_style_bg_color(s_op_keys[i], lit ? lv_color_white() : UI_COLOR_ACCENT, 0);
        set_glyph_color(s_op_keys[i], lit ? UI_COLOR_ACCENT : lv_color_white());
    }

    lv_label_set_text(s_back_label, s_showing_result || s_error ? "C" : LV_SYMBOL_BACKSPACE);

    if (s_error) {
        lv_label_set_text(s_expr, "");
        set_value("Error");
        return;
    }
    if (s_showing_result) {
        // The expression line keeps the sum that was just worked out.
        set_value(s_entry);
        return;
    }
    build_expression(buf, sizeof(buf), true);
    lv_label_set_text(s_expr, buf);
    if (s_entry[0]) {
        set_value(s_entry);
    } else if (s_count > 0) {
        // Waiting for the next number: preview the answer so far.
        double preview;
        if (evaluate(s_numbers, s_ops, s_count, &preview)) {
            format_number(buf, sizeof(buf), preview);
            set_value(buf);
        }
    } else {
        set_value("0");
    }
}

// ---- Keys ----

static void clear_all(void)
{
    s_count = 0;
    s_entry[0] = '\0';
    s_showing_result = false;
    s_error = false;
    lv_label_set_text(s_expr, "");
}

static void start_fresh_if_done(void)
{
    if (s_error) {
        clear_all();
    } else if (s_showing_result) {
        s_showing_result = false;
        s_entry[0] = '\0';
    }
}

static int digit_count(void)
{
    int n = 0;
    for (const char *p = s_entry; *p; p++) {
        n += *p >= '0' && *p <= '9';
    }
    return n;
}

static void press_digit(char d)
{
    start_fresh_if_done();
    if (digit_count() >= MAX_DIGITS) {
        return;
    }
    if (strcmp(s_entry, "0") == 0) {
        s_entry[0] = '\0';
    }
    size_t len = strlen(s_entry);
    s_entry[len] = d;
    s_entry[len + 1] = '\0';
}

static void press_point(void)
{
    start_fresh_if_done();
    if (strchr(s_entry, '.')) {
        return;
    }
    append(s_entry, sizeof(s_entry), s_entry[0] ? "." : "0.");
}

static void press_op(char op)
{
    if (s_error) {
        clear_all();
    }
    if (s_entry[0] == '\0') {
        if (s_count > 0) {
            s_ops[s_count - 1] = op;   // changed their mind
            return;
        }
        snprintf(s_entry, sizeof(s_entry), "0");
    }
    if (s_count == MAX_NUMBERS) {
        return;
    }
    // After "=", carry on from the answer.
    if (s_showing_result) {
        s_showing_result = false;
        lv_label_set_text(s_expr, "");
    }
    s_numbers[s_count] = strtod(s_entry, NULL);
    s_ops[s_count] = op;
    s_count++;
    s_entry[0] = '\0';
}

static void press_equals(void)
{
    if (s_error || s_showing_result || s_count == 0) {
        return;
    }
    int n = s_count;
    if (s_entry[0]) {
        s_numbers[n++] = strtod(s_entry, NULL);
    }
    char expr[64];
    build_expression(expr, sizeof(expr), true);
    if (s_entry[0] == '\0') {
        // A trailing operator with nothing after it is ignored.
        expr[strlen(expr) - 3] = '\0';
    }

    double result;
    if (!evaluate(s_numbers, s_ops, n, &result)) {
        s_error = true;
        s_count = 0;
        s_entry[0] = '\0';
        return;
    }
    lv_label_set_text_fmt(s_expr, "%s =", expr);
    format_number(s_entry, sizeof(s_entry), result);
    s_count = 0;
    s_showing_result = true;
}

static void press_backspace(void)
{
    if (s_error || s_showing_result) {
        clear_all();
        return;
    }
    size_t len = strlen(s_entry);
    if (len > 0) {
        s_entry[len - 1] = '\0';
        if (strcmp(s_entry, "-") == 0) {
            s_entry[0] = '\0';
        }
    } else if (s_count > 0) {
        // Undo the operator: the number before it can be edited again.
        s_count--;
        format_number(s_entry, sizeof(s_entry), s_numbers[s_count]);
    }
}

static void on_key(lv_event_t *e)
{
    char key = (char)(uintptr_t)lv_event_get_user_data(e);
    if (key >= '0' && key <= '9') {
        press_digit(key);
    } else if (key == '.') {
        press_point();
    } else if (key == '=') {
        press_equals();
    } else {
        press_op(key);
    }
    refresh();
}

static void on_backspace(lv_event_t *e)
{
    (void)e;
    press_backspace();
    refresh();
}

static void on_backspace_hold(lv_event_t *e)
{
    (void)e;
    clear_all();
    haptics_play(HAPTIC_TAP);
    refresh();
}

// ---- Building ----

static lv_obj_t *bar(lv_obj_t *parent, int32_t w, int32_t h, int32_t x, int32_t y)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, h < w ? h / 2 : w / 2, 0);
    lv_obj_align(b, LV_ALIGN_CENTER, x, y);
    lv_obj_set_clickable(b, false);
    return b;
}

static void draw_glyph(lv_obj_t *key, char op)
{
    switch (op) {
    case '+':
        bar(key, 18, 3, 0, 0);
        bar(key, 3, 18, 0, 0);
        break;
    case '-':
        bar(key, 18, 3, 0, 0);
        break;
    case '=':
        bar(key, 18, 3, 0, -4);
        bar(key, 18, 3, 0, 4);
        break;
    case '/':
        bar(key, 18, 3, 0, 0);
        bar(key, 5, 5, 0, -7);
        bar(key, 5, 5, 0, 7);
        break;
    case '*': {
        // A plus sign turned 45 degrees.
        lv_obj_t *box = lv_obj_create(key);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, 20, 20);
        lv_obj_center(box);
        lv_obj_set_clickable(box, false);
        bar(box, 18, 3, 0, 0);
        bar(box, 3, 18, 0, 0);
        lv_obj_set_style_transform_pivot_x(box, 10, 0);
        lv_obj_set_style_transform_pivot_y(box, 10, 0);
        lv_obj_set_style_transform_rotation(box, 450, 0);
        break;
    }
    }
    set_glyph_color(key, lv_color_white());
}

static lv_obj_t *add_key(lv_obj_t *parent, char key, int col, int row)
{
    bool is_op = strchr(OPS, key) != NULL;
    lv_color_t color = is_op ? UI_COLOR_ACCENT : key == '=' ? COLOR_EQUALS : COLOR_DIGIT;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, KEY_W, KEY_H);
    lv_obj_set_pos(btn, col * (KEY_W + KEY_GAP), DISPLAY_H + row * (KEY_H + KEY_GAP));
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_color(btn, lv_color_lighten(color, LV_OPA_30), LV_STATE_PRESSED);
    lv_obj_set_gesture_bubble(btn, true);
    lv_obj_add_event_cb(btn, on_key, LV_EVENT_CLICKED, (void *)(uintptr_t)key);

    if (is_op || key == '=') {
        draw_glyph(btn, key);
    } else {
        lv_obj_t *label = ui_label(btn, &lv_font_montserrat_32, lv_color_white());
        char text[2] = {key, '\0'};
        lv_label_set_text(label, text);
        lv_obj_center(label);
    }
    return btn;
}

void calculator_create(lv_obj_t *parent)
{
    lv_obj_t *back = ui_round_button(parent, LV_SYMBOL_BACKSPACE, 40, UI_COLOR_BUTTON);
    lv_obj_set_pos(back, 6, 12);
    lv_obj_set_gesture_bubble(back, true);
    lv_obj_set_ext_click_area(back, 6);
    lv_obj_add_event_cb(back, on_backspace, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(back, on_backspace_hold, LV_EVENT_LONG_PRESSED, NULL);
    s_back_label = lv_obj_get_child(back, 0);

    s_expr = ui_label(parent, &lv_font_montserrat_14, UI_COLOR_DIM);
    lv_obj_set_width(s_expr, VALUE_W);
    lv_obj_set_style_text_align(s_expr, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_expr, 240 - VALUE_W - 8, 0);

    s_value = ui_label(parent, &lv_font_montserrat_48, lv_color_white());
    lv_obj_set_width(s_value, VALUE_W);
    lv_obj_set_style_text_align(s_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(s_value, LV_LABEL_LONG_MODE_CLIP);
    for (int i = 0; i < 16; i++) {
        lv_obj_t *key = add_key(parent, KEYS[i], i % 4, i / 4);
        const char *op = strchr(OPS, KEYS[i]);
        if (op) {
            s_op_keys[op - OPS] = key;
        }
    }
    clear_all();
    refresh();
}
