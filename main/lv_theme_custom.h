#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void lv_theme_custom_init(void);
const lv_style_t *lv_theme_custom_style_screen(void);
const lv_style_t *lv_theme_custom_style_card(void);
const lv_style_t *lv_theme_custom_style_label(void);
const lv_style_t *lv_theme_custom_style_button(void);

#ifdef __cplusplus
}
#endif
