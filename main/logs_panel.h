#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *logs_panel_create(void);
void logs_panel_add_log(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
