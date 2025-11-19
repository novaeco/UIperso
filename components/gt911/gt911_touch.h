#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the GT911 touch controller and register the LVGL input device.
 */
void gt911_init(void);

/**
 * @brief Access the registered LVGL input device (NULL if not initialized).
 */
lv_indev_t *gt911_get_input_device(void);

#ifdef __cplusplus
}
#endif

