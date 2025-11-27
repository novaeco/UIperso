#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    lv_style_t bg;
    lv_style_t card;
    lv_style_t title;
    lv_style_t subtle;
    lv_style_t badge;
    lv_style_t value;
} ui_theme_styles_t;

void ui_theme_init(ui_theme_styles_t *styles);

#ifdef __cplusplus
}
#endif

