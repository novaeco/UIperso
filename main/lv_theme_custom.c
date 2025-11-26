#include "lv_theme_custom.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lv_theme_custom";
static lv_style_t style_screen;
static lv_style_t style_card;
static lv_style_t style_label;
static lv_style_t style_button;
static lv_style_t style_button_active;
static bool theme_ready = false;

static void init_styles(void)
{
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_hex(0x101418));
    lv_style_set_border_width(&style_screen, 0);
    lv_style_set_pad_all(&style_screen, 24);

    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1A1F27));
    lv_style_set_radius(&style_card, 12);
    lv_style_set_pad_all(&style_card, 16);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, lv_color_hex(0x2C3440));

    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, lv_color_hex(0xF3F6FF));
    lv_style_set_text_font(&style_label, &lv_font_montserrat_16);

    lv_style_init(&style_button);
    lv_style_set_bg_color(&style_button, lv_color_hex(0x2D84FF));
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_radius(&style_button, 10);
    lv_style_set_border_width(&style_button, 0);
    lv_style_set_pad_left(&style_button, 8);
    lv_style_set_pad_right(&style_button, 8);
    lv_style_set_pad_top(&style_button, 4);
    lv_style_set_pad_bottom(&style_button, 4);

    lv_style_init(&style_button_active);
    lv_style_set_bg_color(&style_button_active, lv_color_hex(0x4EA1FF));
    lv_style_set_bg_opa(&style_button_active, LV_OPA_COVER);
    lv_style_set_border_width(&style_button_active, 2);
    lv_style_set_border_color(&style_button_active, lv_color_hex(0x82C7FF));
    lv_style_set_shadow_width(&style_button_active, 10);
    lv_style_set_shadow_color(&style_button_active, lv_color_hex(0x4EA1FF));
}

void lv_theme_custom_init(void)
{
    if (theme_ready)
    {
        return;
    }

    int64_t start_us = esp_timer_get_time();
    init_styles();

    vTaskDelay(pdMS_TO_TICKS(1));

    lv_obj_t *act = lv_scr_act();
    if (act)
    {
        lv_obj_add_style(act, &style_screen, LV_PART_MAIN);
    }

    theme_ready = true;
    ESP_LOGI(TAG, "lv_theme_custom_init took %lld ms", (long long)((esp_timer_get_time() - start_us) / 1000));
}

static const lv_style_t *ensure_style_ready(const lv_style_t *style)
{
    if (!theme_ready)
    {
        lv_theme_custom_init();
    }
    return style;
}

const lv_style_t *lv_theme_custom_style_screen(void)
{
    return ensure_style_ready(&style_screen);
}

const lv_style_t *lv_theme_custom_style_card(void)
{
    return ensure_style_ready(&style_card);
}

const lv_style_t *lv_theme_custom_style_label(void)
{
    return ensure_style_ready(&style_label);
}

const lv_style_t *lv_theme_custom_style_button(void)
{
    return ensure_style_ready(&style_button);
}

const lv_style_t *lv_theme_custom_style_button_active(void)
{
    return ensure_style_ready(&style_button_active);
}
