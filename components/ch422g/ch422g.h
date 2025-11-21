#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the CH422G-compatible IO extension and the shared I2C bus.
 */
esp_err_t ch422g_init(void);

/**
 * @brief Determine if the IO extension responded correctly on the I2C bus.
 */
bool ch422g_is_available(void);

/**
 * @brief Control LCD backlight (true = ON).
 */
esp_err_t ch422g_set_backlight(bool on);

/**
 * @brief Control LCD main power / VCOM switch (true = enabled).
 */
esp_err_t ch422g_set_lcd_power(bool on);

/**
 * @brief Assert or release the GT911 reset line (true = reset asserted).
 */
esp_err_t ch422g_set_touch_reset(bool asserted);

/**
 * @brief Select USB (true) or CAN (false) interface via CH422G controlled switch.
 *        Optional helper, safe to call even if the board does not route these lines.
 */
esp_err_t ch422g_select_usb(bool usb_selected);

/**
 * @brief Drive the µSD card chip-select. When @a asserted is true CS is driven active.
 */
esp_err_t ch422g_set_sdcard_cs(bool asserted);

/**
 * @brief Indicate whether the µSD chip-select line is routed to the IO extension.
 */
bool ch422g_sdcard_cs_available(void);

#ifdef __cplusplus
}
#endif

