#include "dashboard.h"

#include <inttypes.h>

#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_system.h"

#include "lv_theme_custom.h"

#define DASHBOARD_CHART_POINTS 60
#define DASHBOARD_UPDATE_PERIOD_MS 1000

static lv_obj_t *heap_label = NULL;
static lv_obj_t *psram_label = NULL;
static lv_obj_t *fps_label = NULL;
static lv_obj_t *chart = NULL;
static lv_chart_series_t *cpu_series = NULL;
static lv_timer_t *update_timer = NULL;

static void dashboard_update_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!heap_label || !psram_label || !fps_label || !chart || !cpu_series)
    {
        return;
    }

    size_t heap_free = esp_get_free_heap_size();
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    lv_label_set_text_fmt(heap_label, "Heap libre : %u Ko", (unsigned int)(heap_free / 1024));
    lv_label_set_text_fmt(psram_label, "PSRAM libre : %u Ko", (unsigned int)(psram_free / 1024));

    static uint32_t fake_fps = 58;
    fake_fps = (fake_fps >= 62) ? 58 : fake_fps + 1;
    lv_label_set_text_fmt(fps_label, "FPS estimé : ~%" PRIu32, (uint32_t)fake_fps);

    uint32_t sample = esp_random() % 100;
    lv_chart_set_next_value(chart, cpu_series, (lv_coord_t)sample);
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
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, lv_theme_custom_style_screen(), LV_PART_MAIN);
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(screen, 18, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_add_style(title, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(title, "Dashboard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);

    lv_obj_t *chart_card = create_card(screen);
    lv_obj_t *chart_title = lv_label_create(chart_card);
    lv_obj_add_style(chart_title, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(chart_title, "Charge CPU (fictive)");

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

    return screen;
}
