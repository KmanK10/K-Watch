// ST7789 240x240 display on SPI, LEDC backlight, LVGL glue.

#include "display.h"

#include "board_pins.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pmu.h"
#include "touch.h"

#define LCD_HOST           SPI2_HOST
#define LCD_PCLK_HZ        (40 * 1000 * 1000)
// The ST7789 has 320 rows of memory; mounted upside down, the visible area starts at row 80.
#define LCD_Y_GAP          80
#define DRAW_BUF_LINES     40

#define BL_TIMER           LEDC_TIMER_0
#define BL_CHANNEL         LEDC_CHANNEL_0
#define BL_FREQ_HZ         1000
#define DIM_MIN_PERCENT    5

static const char *TAG = "display";

static esp_lcd_panel_handle_t s_panel;
static lv_display_t *s_disp;
static esp_pm_lock_handle_t s_render_lock;
static uint8_t s_brightness = 60;
static bool s_asleep;   // the backlight stays off until display_wake

static bool on_flush_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    (void)io;
    (void)edata;
    lv_display_flush_ready((lv_display_t *)ctx);
    return false;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    (void)disp;
    lv_draw_sw_rgb565_swap(px, lv_area_get_size(area));
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    int16_t x, y;
    if (touch_read(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static uint32_t tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = BL_TIMER,
        .freq_hz = BL_FREQ_HZ,
        .clk_cfg = LEDC_USE_XTAL_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .gpio_num = BOARD_TFT_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

static void backlight_set_duty(uint8_t percent)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL, (uint32_t)percent * 255 / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL);
}

static void panel_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = BOARD_TFT_SCLK,
        .mosi_io_num = BOARD_TFT_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_TFT_WIDTH * DRAW_BUF_LINES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = BOARD_TFT_DC,
        .cs_gpio_num = BOARD_TFT_CS,
        .pclk_hz = LCD_PCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = on_flush_done,
        .user_ctx = s_disp,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, 0, LCD_Y_GAP));
}

esp_err_t display_init(void)
{
    // Render at full speed, then drop back to the minimum clock.
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "render", &s_render_lock));
    backlight_init();

    lv_init();
    lv_tick_set_cb(tick_ms);

    s_disp = lv_display_create(BOARD_TFT_WIDTH, BOARD_TFT_HEIGHT);
    size_t buf_bytes = BOARD_TFT_WIDTH * DRAW_BUF_LINES * sizeof(uint16_t);
    void *buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Out of DMA memory for draw buffers");
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_buffers(s_disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, flush_cb);

    panel_init();

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    ESP_LOGI(TAG, "ST7789 + LVGL %d.%d ready", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR);
    return ESP_OK;
}

void display_sleep(void)
{
    s_asleep = true;
    backlight_set_duty(0);
    pmu_set_backlight_power(false);
    esp_lcd_panel_disp_on_off(s_panel, false);
    esp_lcd_panel_disp_sleep(s_panel, true);
    touch_set_low_power(true);
}

void display_wake(void)
{
    touch_set_low_power(false);
    esp_lcd_panel_disp_sleep(s_panel, false);
    // ST7789 needs 5ms after sleep-out before accepting more commands.
    vTaskDelay(pdMS_TO_TICKS(10));

    esp_pm_lock_acquire(s_render_lock);
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(s_disp);
    esp_pm_lock_release(s_render_lock);

    esp_lcd_panel_disp_on_off(s_panel, true);
    pmu_set_backlight_power(true);
    s_asleep = false;
    backlight_set_duty(s_brightness);
    lv_display_trigger_activity(s_disp);
}

void display_set_brightness(uint8_t percent)
{
    s_brightness = percent > 100 ? 100 : percent;
    if (!s_asleep) {
        backlight_set_duty(s_brightness);
    }
}

void display_set_dimmed(bool dimmed)
{
    if (s_asleep) {
        return;
    }
    uint8_t low = s_brightness / 3;
    backlight_set_duty(dimmed ? (low < DIM_MIN_PERCENT ? DIM_MIN_PERCENT : low) : s_brightness);
}

uint32_t display_run(void)
{
    esp_pm_lock_acquire(s_render_lock);
    uint32_t next = lv_timer_handler();
    esp_pm_lock_release(s_render_lock);
    return next;
}

uint32_t display_inactive_ms(void)
{
    return lv_display_get_inactive_time(s_disp);
}

void display_trigger_activity(void)
{
    lv_display_trigger_activity(s_disp);
}
