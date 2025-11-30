#include "ui_smoke.h"

#include "esp_log.h"
#include <inttypes.h>
#include <stdbool.h>

#include "rgb_lcd.h"

static const char *TAG_SMOKE = "SMOKE_UI";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_label = NULL;
static lv_obj_t *s_flush_label = NULL;
static lv_timer_t *s_timer = NULL;
static uint32_t s_counter = 0;

static lv_obj_t *s_diag_screen = NULL;
static lv_obj_t *s_fallback_screen = NULL;

static void log_state(const char *stage)
{
    lv_display_t *default_disp = lv_display_get_default();
    lv_obj_t *screen = lv_screen_active();
    const uint32_t children = screen ? lv_obj_get_child_cnt(screen) : 0;

    ESP_LOGI(TAG_SMOKE,
             "%s: default_disp=%p active_screen=%p children=%" PRIu32,
             stage,
             (void *)default_disp,
             (void *)screen,
             children);
}

static void ui_smoke_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    ++s_counter;
    const uint32_t flush = rgb_lcd_flush_count_get();

    if (s_label)
    {
        lv_label_set_text_fmt(s_label, "LVGL OK %" PRIu32, s_counter);
    }

    if (s_flush_label)
    {
        lv_label_set_text_fmt(s_flush_label, "flush=%" PRIu32, flush);
    }

    ESP_LOGI(TAG_SMOKE, "ui_tick t=%" PRIu32 "s screen=%p", s_counter, (void *)lv_screen_active());
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

    if (lv_display_get_default() == NULL)
    {
        lv_display_set_default(disp);
        ESP_LOGW(TAG_SMOKE, "Default display was NULL, forcing default=%p", (void *)disp);
    }

    log_state("SMOKE_BEGIN");

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_100, LV_PART_MAIN);

    s_label = lv_label_create(s_screen);
    lv_label_set_text(s_label, "UIperso: LVGL OK");
    lv_obj_set_style_text_color(s_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_label, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_center(s_label);

    s_flush_label = lv_label_create(s_screen);
    lv_label_set_text(s_flush_label, "flush=0");
    lv_obj_set_style_text_color(s_flush_label, lv_color_hex(0xE5E5E5), LV_PART_MAIN);
    lv_obj_align(s_flush_label, LV_ALIGN_BOTTOM_MID, 0, -16);

    lv_screen_load(s_screen);

    if (s_timer == NULL)
    {
        s_timer = lv_timer_create(ui_smoke_timer_cb, 1000, NULL);
        if (s_timer)
        {
            lv_timer_set_repeat_count(s_timer, -1);
        }
    }

    ESP_LOGI(TAG_SMOKE, "SMOKE UI ready (children=%u)", (unsigned)lv_obj_get_child_cnt(s_screen));
    log_state("SMOKE_READY");
}

void ui_smoke_boot_screen(void)
{
    static bool boot_screen_created = false;

    if (boot_screen_created)
    {
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    if (screen == NULL)
    {
        ESP_LOGW(TAG_SMOKE, "Smoke screen skipped: no active LVGL screen");
        return;
    }

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0F172A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_100, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "BOOT OK");
    lv_obj_set_style_text_color(label, lv_color_hex(0xF8FAFC), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_center(label);

    ESP_LOGI(TAG_SMOKE, "UI: smoke screen created");
    boot_screen_created = true;
}

void ui_smoke_diag_screen(void)
{
    if (s_diag_screen)
    {
        lv_screen_load(s_diag_screen);
        ESP_LOGI(TAG_SMOKE, "Diag screen reused");
        return;
    }

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFF0022), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_100, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "LVGL OK");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(label);

    lv_obj_t *rect = lv_obj_create(screen);
    lv_obj_set_size(rect, 120, 80);
    lv_obj_set_style_bg_color(rect, lv_color_hex(0x00A000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rect, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_border_width(rect, 0, LV_PART_MAIN);
    lv_obj_align(rect, LV_ALIGN_TOP_LEFT, 10, 10);

    s_diag_screen = screen;
    lv_screen_load(s_diag_screen);
    ESP_LOGI(TAG_SMOKE, "Diag screen created (red bg + green rect)");
}

void ui_smoke_fallback(void)
{
    if (s_fallback_screen == NULL)
    {
        s_fallback_screen = lv_obj_create(NULL);
        lv_obj_clear_flag(s_fallback_screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(s_fallback_screen, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_fallback_screen, LV_OPA_100, LV_PART_MAIN);

        lv_obj_t *label = lv_label_create(s_fallback_screen);
        lv_label_set_text(label, "UIperso: LVGL OK");
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_center(label);

        ESP_LOGI(TAG_SMOKE, "Fallback smoke screen prepared (children=%u)", (unsigned)lv_obj_get_child_cnt(s_fallback_screen));
    }

    lv_screen_load(s_fallback_screen);
    ESP_LOGI(TAG_SMOKE, "Fallback smoke screen loaded");
}
