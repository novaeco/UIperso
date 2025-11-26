#include "dashboard.h"

#include <inttypes.h>

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

#include "cs8501.h"
#include "lv_theme_custom.h"

#define DASHBOARD_CHART_POINTS 60
#define DASHBOARD_UPDATE_PERIOD_MS 750
#define DASHBOARD_SMOOTHING_WINDOW 8

static const char *TAG = "dashboard";
static lv_obj_t *heap_label = NULL;
static lv_obj_t *psram_label = NULL;
static lv_obj_t *fps_label = NULL;
static lv_obj_t *chart = NULL;
static lv_chart_series_t *cpu_series = NULL;
static lv_timer_t *update_timer = NULL;
static uint16_t smoothing_buffer[DASHBOARD_SMOOTHING_WINDOW] = {0};
static size_t smoothing_index = 0;
static size_t smoothing_count = 0;
static uint32_t smoothing_sum = 0;
static size_t heap_baseline = 0;
static size_t psram_baseline = 0;

static uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max)
{
    if (value < min)
    {
        return min;
    }
    if (value > max)
    {
        return max;
    }
    return value;
}

static void dashboard_update_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!heap_label || !psram_label || !fps_label || !chart || !cpu_series)
    {
        return;
    }

    size_t heap_free = esp_get_free_heap_size();
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    if (heap_baseline == 0)
    {
        heap_baseline = heap_free;
    }
    if (psram_baseline == 0 && psram_free > 0)
    {
        psram_baseline = psram_free;
    }

    lv_label_set_text_fmt(heap_label, "Heap libre : %u Ko", (unsigned int)(heap_free / 1024));
    lv_label_set_text_fmt(psram_label, "PSRAM libre : %u Ko", (unsigned int)(psram_free / 1024));

    static uint32_t fake_fps = 58;
    fake_fps = (fake_fps >= 62) ? 58 : fake_fps + 1;
    lv_label_set_text_fmt(fps_label, "FPS estimé : ~%" PRIu32, (uint32_t)fake_fps);

    uint32_t heap_load_percent = 0;
    if (heap_baseline > 0)
    {
        heap_load_percent = clamp_u32(100 - (uint32_t)((heap_free * 100U) / heap_baseline), 0, 100);
    }

    uint32_t psram_load_percent = 0;
    if (psram_baseline > 0)
    {
        psram_load_percent = clamp_u32(100 - (uint32_t)((psram_free * 100U) / psram_baseline), 0, 100);
    }

    uint32_t battery_percent = 0;
    if (cs8501_has_voltage_reading())
    {
        float battery_voltage = cs8501_get_battery_voltage();
        if (!isnan(battery_voltage))
        {
            float battery_ratio = (battery_voltage - 3.3f) / (4.2f - 3.3f);
            if (battery_ratio < 0.0f)
            {
                battery_ratio = 0.0f;
            }
            if (battery_ratio > 1.0f)
            {
                battery_ratio = 1.0f;
            }
            battery_percent = (uint32_t)(battery_ratio * 100.0f);

            if (cs8501_has_charge_status() && cs8501_is_charging())
            {
                battery_percent = clamp_u32(battery_percent + 5U, 0, 100);
            }
        }
    }

    uint32_t sample = (heap_load_percent + psram_load_percent + battery_percent) / 3U;

    smoothing_sum -= smoothing_buffer[smoothing_index];
    smoothing_buffer[smoothing_index] = (uint16_t)sample;
    smoothing_sum += smoothing_buffer[smoothing_index];
    smoothing_index = (smoothing_index + 1) % DASHBOARD_SMOOTHING_WINDOW;
    if (smoothing_count < DASHBOARD_SMOOTHING_WINDOW)
    {
        smoothing_count++;
    }

    uint32_t smoothed_sample = smoothing_sum / smoothing_count;

    lv_chart_set_next_value(chart, cpu_series, (lv_coord_t)smoothed_sample);
}

static lv_obj_t *create_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, lv_theme_custom_style_card(), LV_PART_MAIN);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_style_pad_row(card, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(card, 8, LV_PART_MAIN);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    return card;
}

lv_obj_t *dashboard_create(void)
{
    int64_t start_us = esp_timer_get_time();
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, lv_theme_custom_style_screen(), LV_PART_MAIN);
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(screen, 18, LV_PART_MAIN);
    vTaskDelay(pdMS_TO_TICKS(1));

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_add_style(title, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(title, "Dashboard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    vTaskDelay(pdMS_TO_TICKS(1));

    lv_obj_t *chart_card = create_card(screen);
    lv_obj_t *chart_title = lv_label_create(chart_card);
    lv_obj_add_style(chart_title, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(chart_title, "Charge système estimée");

    chart = lv_chart_create(chart_card);
    lv_obj_set_size(chart, 600, 200);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, DASHBOARD_CHART_POINTS);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    cpu_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_CYAN), LV_CHART_AXIS_PRIMARY_Y);
    for (uint32_t i = 0; i < DASHBOARD_CHART_POINTS; i++)
    {
        lv_chart_set_value_by_id(chart, cpu_series, i, 20);
    }
    vTaskDelay(pdMS_TO_TICKS(1));

    lv_obj_t *stats_card = create_card(screen);
    heap_label = lv_label_create(stats_card);
    lv_obj_add_style(heap_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(heap_label, "Heap libre : --");

    psram_label = lv_label_create(stats_card);
    lv_obj_add_style(psram_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(psram_label, "PSRAM libre : --");

    fps_label = lv_label_create(stats_card);
    lv_obj_add_style(fps_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(fps_label, "FPS estimé : --");

    if (!update_timer)
    {
        update_timer = lv_timer_create(dashboard_update_cb, DASHBOARD_UPDATE_PERIOD_MS, NULL);
    }

    ESP_LOGI(TAG, "dashboard_create took %lld ms", (long long)((esp_timer_get_time() - start_us) / 1000));

    return screen;
}
