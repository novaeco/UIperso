#include "ui_manager.h"

#include "esp_log.h"
#include "lvgl.h"

#include "dashboard.h"
#include "logs_panel.h"
#include "lv_theme_custom.h"
#include "system_panel.h"

static const char *TAG = "ui_manager";

static lv_obj_t *dashboard_screen = NULL;
static lv_obj_t *system_screen = NULL;
static lv_obj_t *logs_screen = NULL;

static void nav_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }
    lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(e);
    if (target)
    {
        lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
    }
}

static void create_navbar(lv_obj_t *screen)
{
    if (!screen)
    {
        return;
    }

    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, lv_pct(100), 70);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1A1F27), LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x2C3440), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    struct
    {
        const char *label;
        lv_obj_t *target;
    } buttons[] = {
        {"Dashboard", dashboard_screen},
        {"Système", system_screen},
        {"Logs", logs_screen},
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++)
    {
        lv_obj_t *btn = lv_btn_create(bar);
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, lv_theme_custom_style_button(), LV_PART_MAIN);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_add_event_cb(btn, nav_button_event_cb, LV_EVENT_CLICKED, buttons[i].target);

        lv_obj_t *label = lv_label_create(btn);
        lv_obj_add_style(label, lv_theme_custom_style_label(), LV_PART_MAIN);
        lv_label_set_text(label, buttons[i].label);
        lv_obj_center(label);
    }
}

void ui_manager_init(void)
{
    lv_theme_custom_init();

    dashboard_screen = dashboard_create();
    system_screen = system_panel_create();
    logs_screen = logs_panel_create();

    create_navbar(dashboard_screen);
    create_navbar(system_screen);
    create_navbar(logs_screen);

    lv_scr_load(dashboard_screen);

    logs_panel_add_log("UI initialisée");
    ESP_LOGI(TAG, "Interface LVGL prête");
}
