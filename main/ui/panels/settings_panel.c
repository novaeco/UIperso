#include "settings_panel.h"

#include <stdarg.h>

#include "esp_log.h"

static const char *TAG = "settings_panel";
static const ui_theme_styles_t *s_theme = NULL;
static const system_status_t *s_status = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_touch_switch = NULL;
static lv_obj_t *s_sd_switch = NULL;

static void set_label_text(lv_obj_t *label, const char *fmt, ...)
{
    if (!label || !fmt)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    lv_label_set_text_vfmt(label, fmt, args);
    va_end(args);
}

static void switch_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    const bool on = lv_obj_has_state(target, LV_STATE_CHECKED);
    const char *name = (target == s_touch_switch) ? "touch" : "scan SD";
    ESP_LOGI(TAG, "Toggle %s => %s", name, on ? "ON" : "OFF");
}

lv_obj_t *settings_panel_create(lv_obj_t *parent, const ui_theme_styles_t *theme, const system_status_t *status_ref)
{
    s_theme = theme;
    s_status = status_ref;

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &s_theme->bg, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);

    lv_obj_t *header = lv_label_create(panel);
    lv_obj_add_style(header, &s_theme->title, LV_PART_MAIN);
    lv_label_set_text(header, "Réglages");

    lv_obj_t *touch_row = lv_obj_create(panel);
    lv_obj_remove_style_all(touch_row);
    lv_obj_set_width(touch_row, lv_pct(100));
    lv_obj_set_layout(touch_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(touch_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(touch_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *touch_label = lv_label_create(touch_row);
    lv_obj_add_style(touch_label, &s_theme->value, LV_PART_MAIN);
    lv_label_set_text(touch_label, "Tactile activé");

    s_touch_switch = lv_switch_create(touch_row);
    lv_obj_add_event_cb(s_touch_switch, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_state(s_touch_switch, LV_STATE_CHECKED);

    lv_obj_t *sd_row = lv_obj_create(panel);
    lv_obj_remove_style_all(sd_row);
    lv_obj_set_width(sd_row, lv_pct(100));
    lv_obj_set_layout(sd_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sd_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sd_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *sd_label = lv_label_create(sd_row);
    lv_obj_add_style(sd_label, &s_theme->value, LV_PART_MAIN);
    lv_label_set_text(sd_label, "Scan microSD");

    s_sd_switch = lv_switch_create(sd_row);
    lv_obj_add_event_cb(s_sd_switch, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_state(s_sd_switch, LV_STATE_CHECKED);

    s_status_label = lv_label_create(panel);
    lv_obj_add_style(s_status_label, &s_theme->subtle, LV_PART_MAIN);

    settings_panel_update(status_ref);
    return panel;
}

void settings_panel_update(const system_status_t *status_ref)
{
    const system_status_t *status = status_ref ? status_ref : s_status;
    if (!status)
    {
        return;
    }

    set_label_text(s_status_label,
                   "microSD: %s | tactile: %s",
                   status->sd_mounted ? "montée" : "absente",
                   status->touch_available ? "ON" : "OFF");
}

