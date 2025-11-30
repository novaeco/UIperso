#include "dashboard_panel.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>

static const ui_theme_styles_t *s_theme = NULL;
static const system_status_t *s_status = NULL;
static const reptiles_data_t *s_reptiles = NULL;
static const char *s_storage_mount = NULL;

static lv_obj_t *s_summary = NULL;
static lv_obj_t *s_wifi_label = NULL;
static lv_obj_t *s_sd_label = NULL;
static lv_obj_t *s_count_label = NULL;

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

static void set_label(lv_obj_t *label, const char *fmt, ...)
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

lv_obj_t *dashboard_panel_create(lv_obj_t *parent,
                                 const ui_theme_styles_t *theme,
                                 const system_status_t *status_ref,
                                 const reptiles_data_t *reptiles,
                                 const reptile_wifi_state_t *wifi_state,
                                 const char *storage_mount)
{
    s_theme = theme;
    s_status = status_ref;
    s_reptiles = reptiles;
    s_storage_mount = storage_mount;

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &s_theme->bg, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);

    lv_obj_t *header = lv_label_create(panel);
    lv_obj_add_style(header, &s_theme->title, LV_PART_MAIN);
    lv_label_set_text(header, "Tableau de bord");

    s_summary = create_card(panel, "Synthèse");
    s_count_label = lv_label_create(s_summary);
    lv_obj_add_style(s_count_label, &s_theme->value, LV_PART_MAIN);

    s_wifi_label = lv_label_create(s_summary);
    lv_obj_add_style(s_wifi_label, &s_theme->value, LV_PART_MAIN);

    s_sd_label = lv_label_create(s_summary);
    lv_obj_add_style(s_sd_label, &s_theme->value, LV_PART_MAIN);

    dashboard_panel_update(status_ref, reptiles, wifi_state);
    return panel;
}

void dashboard_panel_update(const system_status_t *status_ref, const reptiles_data_t *reptiles, const reptile_wifi_state_t *wifi_state)
{
    if (status_ref)
    {
        s_status = status_ref;
    }
    if (reptiles)
    {
        s_reptiles = reptiles;
    }
    const int count = s_reptiles ? s_reptiles->reptile_count : 0;
    set_label(s_count_label, "Reptiles gérés: %d", count);

    const bool wifi_ok = s_status ? s_status->wifi_connected : false;
    const char *ssid = (s_status && s_status->wifi_connected) ? s_status->wifi_ssid : "non configuré";
    set_label(s_wifi_label, "Wi-Fi: %s (%s)", wifi_ok ? "connecté" : "hors ligne", ssid);

    const bool sd_ok = s_status ? s_status->sd_mounted : false;
    set_label(s_sd_label, "Stockage (%s): %s", s_storage_mount ? s_storage_mount : "?", sd_ok ? "ok" : "absent");
}
