#ifndef LV_CONF_H
#define LV_CONF_H

/* Enable simple include path so ESP-IDF builds find this configuration */
#define LV_CONF_INCLUDE_SIMPLE    1

/*====================
 * Display settings
 *====================*/
#define LV_HOR_RES_MAX            1024
#define LV_VER_RES_MAX            600
#define LV_COLOR_DEPTH            16
#define LV_COLOR_16_SWAP          0
#define LV_COLOR_SCREEN_TRANSP    0

/*====================
 * Memory settings
 *====================*/
#define LV_MEM_SIZE               (128U * 1024U)
#define LV_MEMCPY_MEMSET_STD      1
#define LV_MEM_ADR                0

/*====================
 * Logging
 *====================*/
#define LV_USE_LOG                1
#define LV_LOG_LEVEL              LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF             1

/*====================
 * Feature configuration
 *====================*/
#define LV_USE_PERF_MONITOR       1
#define LV_USE_PERF_MONITOR_POS   LV_ALIGN_BOTTOM_RIGHT

#define LV_USE_DRAW_SW            1
#define LV_DRAW_SW_MAX_BUF_SIZE   (128U * 1024U)

/* Widgets needed for the upcoming UI */
#define LV_USE_LABEL              1
#define LV_USE_BTN                1
#define LV_USE_BTNMATRIX          1
#define LV_USE_CANVAS             1
#define LV_USE_CHART              1
#define LV_USE_TEXTAREA           1
#define LV_USE_SLIDER             1
#define LV_USE_SWITCH             1
#define LV_USE_ARC                1
#define LV_USE_IMAGE              1
#define LV_USE_IMGBTN             1
#define LV_USE_LIST               1
#define LV_USE_MSGBOX             1

/*====================
 * OS and tick configuration
 *====================*/
#define LV_USE_OS                 LV_OS_FREERTOS
#define LV_TICK_CUSTOM            0
#define LV_TICK_CUSTOM_INCLUDE    "esp_timer.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR   (esp_timer_get_time() / 1000ULL)

/*====================
 * Fonts
 *====================*/
#define LV_FONT_MONTSERRAT_14     1
#define LV_FONT_MONTSERRAT_16     1
#define LV_FONT_MONTSERRAT_20     1
#define LV_FONT_MONTSERRAT_24     1
#define LV_FONT_MONTSERRAT_28     1

#endif /* LV_CONF_H */
