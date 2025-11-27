#include "ui_smoke.h"

#include "esp_log.h"

#include "rgb_lcd.h"

static const char *TAG_SMOKE = "SMOKE_UI";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_label = NULL;
static lv_obj_t *s_flush_label = NULL;
static lv_timer_t *s_timer = NULL;
static uint32_t s_counter = 0;

static void ui_smoke_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    ++s_counter;
    const uint32_t flush = rgb_lcd_flush_count_get();

    if (s_label)
    {
        lv_label_set_text_fmt(s_label, "LVGL OK %lu", (unsigned long)s_counter);
    }

    if (s_flush_label)
    {
        lv_label_set_text_fmt(s_flush_label, "flush=%lu", (unsigned long)flush);
    }
}

void ui_smoke_init(lv_display_t *disp)
{
    if (s_screen != NULL)
    {
        ESP_LOGI(TAG_SMOKE, "Smoke UI already initialized");
        return;
    }

    if (disp == NULL)
    {
        ESP_LOGW(TAG_SMOKE, "Smoke UI skipped: no display");
        return;
    }

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x203040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_100, LV_PART_MAIN);

    lv_obj_t *banner = lv_obj_create(s_screen);
    lv_obj_set_size(banner, lv_pct(100), 60);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0xFFD166), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(banner, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_border_width(banner, 0, LV_PART_MAIN);
    lv_obj_align(banner, LV_ALIGN_BOTTOM_MID, 0, 0);

    s_label = lv_label_create(s_screen);
    lv_label_set_text(s_label, "LVGL OK 0");
    lv_obj_set_style_text_color(s_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_label, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_center(s_label);

    s_flush_label = lv_label_create(banner);
    lv_label_set_text(s_flush_label, "flush=0");
    lv_obj_set_style_text_color(s_flush_label, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_align(s_flush_label, LV_ALIGN_CENTER, 0, 0);

    lv_screen_load(s_screen);

    if (s_timer == NULL)
    {
        s_timer = lv_timer_create(ui_smoke_timer_cb, 1000, NULL);
        if (s_timer)
        {
            lv_timer_set_repeat_count(s_timer, -1);
        }
    }

    ESP_LOGI(TAG_SMOKE, "SMOKE UI ready");
}
