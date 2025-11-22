#pragma once

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the GT911 touch controller and register the LVGL input device.
 *
 * @param disp LVGL display handle to bind the input device to (fallback to default if NULL).
 */
void gt911_init(lv_display_t *disp);
bool gt911_is_initialized(void);

/**
 * @brief Access the registered LVGL input device (NULL if not initialized).
 */
lv_indev_t *gt911_get_input_device(void);

#ifdef __cplusplus
}
#endif

