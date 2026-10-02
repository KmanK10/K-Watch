#pragma once

// Flash storage stand-in: nothing is ever saved, so settings start from defaults.

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;

static inline esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *h)
{
    (void)ns;
    (void)mode;
    (void)h;
    return ESP_FAIL;
}

static inline esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len)
{
    (void)h, (void)key, (void)out, (void)len;
    return ESP_FAIL;
}

static inline esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *v, size_t len)
{
    (void)h, (void)key, (void)v, (void)len;
    return ESP_FAIL;
}

static inline esp_err_t nvs_commit(nvs_handle_t h)
{
    (void)h;
    return ESP_FAIL;
}

static inline void nvs_close(nvs_handle_t h)
{
    (void)h;
}
