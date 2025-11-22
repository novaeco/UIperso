#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_manager_init(void);
void ui_manager_set_degraded(bool degraded);

#ifdef __cplusplus
}
#endif
