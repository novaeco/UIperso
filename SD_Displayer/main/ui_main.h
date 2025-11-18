#pragma once
#include "lvgl.h"
#include "wifi_manager.h"
#include "sd.h"

typedef struct {
    lv_obj_t *wifi_icon;
    lv_obj_t *sd_icon;
    lv_obj_t *battery_icon;
    lv_obj_t *clock_label;
    lv_obj_t *image;
    lv_obj_t *title;
    char **images;
    int image_count;
    int current_index;
} ui_context_t;

void ui_init(lv_display_t *display, ui_context_t *ctx);
void ui_update_status(ui_context_t *ctx);
