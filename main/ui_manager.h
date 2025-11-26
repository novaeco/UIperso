#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ui_manager_init(void);
esp_err_t ui_manager_init_step1_theme(void);
esp_err_t ui_manager_init_step2_screens(void);
esp_err_t ui_manager_init_step3_finalize(void);
void ui_manager_set_degraded(bool degraded);

#ifdef __cplusplus
}
#endif
