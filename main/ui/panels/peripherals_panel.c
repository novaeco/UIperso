#include "peripherals_panel.h"

#include <inttypes.h>
#include <stdarg.h>

static const ui_theme_styles_t *s_theme = NULL;
static const system_status_t *s_status = NULL;
static lv_obj_t *s_bus_card = NULL;
static lv_obj_t *s_power_card = NULL;
static lv_obj_t *s_bus_label = NULL;
static lv_obj_t *s_touch_label = NULL;
static lv_obj_t *s_storage_label = NULL;
static lv_obj_t *s_power_label = NULL;

static lv_obj_t *create_card(lv_obj_t *parent, const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &s_theme->card, LV_PART_MAIN);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, LV_PART_MAIN);

    if (title)
    {
        lv_obj_t *label = lv_label_create(card);
        lv_obj_add_style(label, &s_theme->title, LV_PART_MAIN);
        lv_label_set_text(label, title);
    }
    return card;
}

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

lv_obj_t *peripherals_panel_create(lv_obj_t *parent, const ui_theme_styles_t *theme, const system_status_t *status_ref)
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
    lv_label_set_text(header, "Périphériques");

    s_bus_card = create_card(panel, "Bus & IO");
    s_bus_label = lv_label_create(s_bus_card);
    lv_obj_add_style(s_bus_label, &s_theme->value, LV_PART_MAIN);

    s_touch_label = lv_label_create(s_bus_card);
    lv_obj_add_style(s_touch_label, &s_theme->value, LV_PART_MAIN);

    s_storage_label = lv_label_create(s_bus_card);
    lv_obj_add_style(s_storage_label, &s_theme->value, LV_PART_MAIN);

    s_power_card = create_card(panel, "Alimentation");
    s_power_label = lv_label_create(s_power_card);
    lv_obj_add_style(s_power_label, &s_theme->value, LV_PART_MAIN);

    peripherals_panel_update(status_ref);
    return panel;
}

void peripherals_panel_update(const system_status_t *status_ref)
{
    const system_status_t *status = status_ref ? status_ref : s_status;
    if (!status)
    {
        return;
    }

    set_label_text(s_bus_label,
                   "CAN: %s / RS485: %s",
                   status->can_ok ? "OK" : "KO",
                   status->rs485_ok ? "OK" : "KO");
    set_label_text(s_touch_label, "Tactile: %s", status->touch_available ? "disponible" : "indisponible");
    set_label_text(s_storage_label, "Stockage: %s", status->sd_mounted ? "microSD montée" : "aucune microSD");

    if (status->power_ok && status->power_telemetry_available)
    {
        set_label_text(s_power_label, "Batterie %.2f V (%s)", status->vbat, status->charging ? "charge" : "repos");
    }
    else
    {
        set_label_text(s_power_label, "Batterie non disponible");
    }
}

