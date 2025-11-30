#include "logs_panel_view.h"

#include <stdio.h>
#include <string.h>

static const ui_theme_styles_t *s_theme = NULL;
static reptiles_data_t *s_data = NULL;
static lv_obj_t *s_list = NULL;

static void add_log_line(const char *title, const reptile_log_entry_t *entry)
{
    if (!s_list || !title || !entry)
    {
        return;
    }
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, &s_theme->card, LV_PART_MAIN);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(row, 8, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text_fmt(label, "%s - %s", title, entry->date);

    lv_obj_t *desc = lv_label_create(row);
    lv_obj_add_style(desc, &s_theme->subtle, LV_PART_MAIN);
    lv_label_set_text(desc, entry->description);
}

static const reptile_log_entry_t *last_entry(const reptile_log_entry_t *arr, int count)
{
    if (count <= 0)
    {
        return NULL;
    }
    return &arr[count - 1];
}

static void rebuild(void)
{
    if (!s_list || !s_data)
    {
        return;
    }
    lv_obj_clean(s_list);

    for (int i = 0; i < s_data->reptile_count; i++)
    {
        const reptile_t *rep = &s_data->reptiles[i];
        const reptile_log_entry_t *feed = last_entry(rep->feedings, rep->feedings_count);
        const reptile_log_entry_t *shed = last_entry(rep->sheddings, rep->sheddings_count);
        const reptile_log_entry_t *health = last_entry(rep->health_checks, rep->health_count);

        if (feed)
        {
            char title[64];
            snprintf(title, sizeof(title), "%s - dernier repas", rep->name);
            add_log_line(title, feed);
        }
        if (shed)
        {
            char title[64];
            snprintf(title, sizeof(title), "%s - dernière mue", rep->name);
            add_log_line(title, shed);
        }
        if (health)
        {
            char title[64];
            snprintf(title, sizeof(title), "%s - contrôle santé", rep->name);
            add_log_line(title, health);
        }
    }
}

lv_obj_t *logs_panel_view_create(lv_obj_t *parent, const ui_theme_styles_t *theme, reptiles_data_t *data)
{
    s_theme = theme;
    s_data = data;

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &s_theme->bg, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);

    lv_obj_t *header = lv_label_create(panel);
    lv_obj_add_style(header, &s_theme->title, LV_PART_MAIN);
    lv_label_set_text(header, "Documents / Logs");

    s_list = lv_obj_create(panel);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_width(s_list, lv_pct(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_layout(s_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);

    rebuild();
    return panel;
}

void logs_panel_view_update(reptiles_data_t *data)
{
    if (data)
    {
        s_data = data;
    }
    rebuild();
}
