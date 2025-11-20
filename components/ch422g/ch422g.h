#pragma once

#include <stdbool.h>

#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the CH422G GPIO expander and the shared I2C bus.
 */
esp_err_t ch422g_init(void);

/**
 * @brief Obtain the I2C port used by the CH422G + GT911 devices.
 */
i2c_port_t ch422g_get_i2c_port(void);

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

#ifdef __cplusplus
}
#endif

