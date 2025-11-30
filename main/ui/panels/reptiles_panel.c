#include "reptiles_panel.h"

#include <stdio.h>
#include <string.h>

static const ui_theme_styles_t *s_theme = NULL;
static reptiles_data_t *s_data = NULL;
static reptile_save_cb_t s_save_cb = NULL;
static lv_obj_t *s_list = NULL;

static void refresh_list(void)
{
    if (!s_list || !s_data)
    {
        return;
    }

    lv_obj_clean(s_list);
    for (int i = 0; i < s_data->reptile_count; i++)
    {
        const reptile_t *rep = &s_data->reptiles[i];
        lv_obj_t *btn = lv_btn_create(s_list);
        lv_obj_add_style(btn, &s_theme->card, LV_PART_MAIN);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, LV_SIZE_CONTENT);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text_fmt(lbl, "%s (%s) - %.1f g", rep->name, rep->species, rep->weight);
        lv_obj_center(lbl);
    }
}

static void add_reptile_event(lv_event_t *e)
{
    (void)e;
    if (!s_data)
    {
        return;
    }

    if (s_data->reptile_count >= REPTILE_MAX_REPTILES)
    {
        return;
    }

    reptile_t rep = {0};
    snprintf(rep.name, sizeof(rep.name), "Reptile %d", s_data->reptile_count + 1);
    strlcpy(rep.species, "Espèce", sizeof(rep.species));
    rep.weight = 0.0f;
    rep.target_temp = 28.0f;
    rep.healthy = true;

    reptile_core_add_reptile(s_data, &rep);
    refresh_list();

    if (s_save_cb)
    {
        s_save_cb("Ajout d'un reptile");
    }
}

lv_obj_t *reptiles_panel_create(lv_obj_t *parent,
                                const ui_theme_styles_t *theme,
                                reptiles_data_t *data,
                                reptile_save_cb_t save_cb)
{
    s_theme = theme;
    s_data = data;
    s_save_cb = save_cb;

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &s_theme->bg, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);

    lv_obj_t *header = lv_label_create(panel);
    lv_obj_add_style(header, &s_theme->title, LV_PART_MAIN);
    lv_label_set_text(header, "Reptiles");

    lv_obj_t *add_btn = lv_btn_create(panel);
    lv_obj_add_style(add_btn, &s_theme->badge, LV_PART_MAIN);
    lv_obj_set_width(add_btn, lv_pct(100));
    lv_obj_add_event_cb(add_btn, add_reptile_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *add_lbl = lv_label_create(add_btn);
    lv_label_set_text(add_lbl, LV_SYMBOL_PLUS " Ajouter un reptile");
    lv_obj_center(add_lbl);

    s_list = lv_obj_create(panel);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_width(s_list, lv_pct(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_layout(s_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(s_list, LV_SCROLL_SNAP_CENTER);

    refresh_list();
    return panel;
}

void reptiles_panel_update(const reptiles_data_t *data)
{
    if (data)
    {
        s_data = (reptiles_data_t *)data;
    }
    refresh_list();
}
