#include <inttypes.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "k-watch";

static void log_chip_info(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "Chip: %s rev %d.%d, %d cores, %s%s",
             CONFIG_IDF_TARGET,
             chip.revision / 100, chip.revision % 100,
             chip.cores,
             (chip.features & CHIP_FEATURE_BLE) ? "BLE " : "",
             (chip.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi" : "");
    ESP_LOGI(TAG, "Flash: %" PRIu32 " MB, PSRAM: %u MB",
             flash_size / (1024 * 1024),
             (unsigned)(esp_psram_get_size() / (1024 * 1024)));
}

static void configure_power(void)
{
    // Light sleep stays off until the wake sources (button, touch, IMU) exist,
    // otherwise the USB console drops and nothing can wake the watch.
    esp_pm_config_t pm = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        .light_sleep_enable = false,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm));
}

void app_main(void)
{
    log_chip_info();
    configure_power();

    while (true) {
        ESP_LOGI(TAG, "alive");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
