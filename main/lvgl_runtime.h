#pragma once

#include "esp_err.h"
#include "lvgl.h"

/**
 * @brief Start the LVGL runtime (tick source + handler task).
 *
 * The tick uses a single esp_timer periodic callback and the handler runs in a
 * dedicated FreeRTOS task. Subsequent calls are ignored once started.
 */
esp_err_t lvgl_runtime_start(lv_display_t *disp);

/**
 * @brief Return the cumulative tick callback count (incremented once per tick).
 */
uint32_t lvgl_tick_alive_count(void);

