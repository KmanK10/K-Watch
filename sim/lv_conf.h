// LVGL settings for the PC simulator. Mirrors the watch's LVGL settings in
// sdkconfig.defaults, so screens look and behave the same.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_FORMAT_DEFAULT     LV_COLOR_FORMAT_RGB565
#define LV_DPI_DEF                  130
#define LV_DEF_REFR_PERIOD          33
#define LV_USE_OS                   LV_OS_NONE

#define LV_USE_STDLIB_MALLOC        LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING        LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF       LV_STDLIB_BUILTIN
// The watch has 48 KB. Pointers are twice as big on a PC, so objects take more room here.
#define LV_MEM_SIZE                 (512 * 1024)

#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE  (24 * 1024)
#define LV_OBJ_STYLE_CACHE          1

#define LV_FONT_MONTSERRAT_14       1
#define LV_FONT_MONTSERRAT_16       1
#define LV_FONT_MONTSERRAT_20       1
#define LV_FONT_MONTSERRAT_32       1
#define LV_FONT_MONTSERRAT_48       1

#define LV_USE_SNAPSHOT             1

#define LV_USE_LOG                  1
#define LV_LOG_LEVEL                LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF               1

#endif
