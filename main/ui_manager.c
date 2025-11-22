#include "ui_manager.h"

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

#include "dashboard.h"
#include "logs_panel.h"
#include "lv_theme_custom.h"
#include "system_panel.h"

/*
 * UI init stability notes:
 * - Previously, calling ui_manager_init() without a valid default LVGL display
 *   could trigger an LVGL assert, which the runtime escalated into
 *   esp_restart_noos (panic reset). We now validate the display upfront and
 *   propagate a non-fatal error so the system can stay alive in degraded mode
 *   when the panel or touch are missing.
 * - Re-entrance is now tolerated: calling ui_manager_init() twice simply logs
 *   a warning and returns success instead of rebuilding screens (which could
 *   re-trigger LVGL internal assertions on duplicate objects).
 */

static const char *TAG = "ui_manager";

static lv_obj_t *dashboard_screen = NULL;
static lv_obj_t *system_screen = NULL;
static lv_obj_t *logs_screen = NULL;
static lv_obj_t *active_nav_button = NULL;
static lv_obj_t *dashboard_nav_button = NULL;
static lv_obj_t *system_nav_button = NULL;
static lv_obj_t *logs_nav_button = NULL;
static lv_obj_t *dashboard_alert = NULL;
static lv_obj_t *system_alert = NULL;
static lv_obj_t *logs_alert = NULL;
static bool s_degraded = false;
static bool s_ui_ready = false;

static void degraded_details_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    logs_panel_add_log("Mode dégradé : vérifier CH422G, affichage RGB et tactile GT911");
}

static lv_obj_t *create_degraded_alert(lv_obj_t *screen)
{
    if (screen == NULL)
    {
        return NULL;
    }

    lv_obj_t *alert = lv_obj_create(screen);
    lv_obj_remove_style_all(alert);
    lv_obj_set_width(alert, lv_pct(100));
    lv_obj_set_style_radius(alert, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(alert, lv_color_hex(0xC62828), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(alert, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_pad_all(alert, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(alert, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(alert, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(alert, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(alert, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(alert, lv_color_hex(0x5D1A1A), LV_PART_MAIN);
    lv_obj_set_layout(alert, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(alert, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(alert, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(alert, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *label = lv_label_create(alert);
    lv_obj_add_style(label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(label, "Mode dégradé : matériel LCD/tactile partiellement disponible");

    lv_obj_t *btn = lv_btn_create(alert);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, lv_theme_custom_style_button(), LV_PART_MAIN);
    lv_obj_add_style(btn, lv_theme_custom_style_button_active(), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_pad_left(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_top(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(btn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, degraded_details_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_obj_add_style(btn_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(btn_label, "Détails");
    lv_obj_center(btn_label);

    lv_obj_add_flag(alert, LV_OBJ_FLAG_HIDDEN);

    return alert;
}

static void set_alert_visibility(lv_obj_t *alert, bool visible)
{
    if (alert == NULL)
    {
        return;
    }

    if (visible)
    {
        lv_obj_clear_flag(alert, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(alert, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_degraded_alerts(void)
{
    set_alert_visibility(dashboard_alert, s_degraded);
    set_alert_visibility(system_alert, s_degraded);
    set_alert_visibility(logs_alert, s_degraded);
}

static lv_obj_t *get_nav_button_for_screen(lv_obj_t *screen)
{
    if (screen == dashboard_screen)
    {
        return dashboard_nav_button;
    }
    if (screen == system_screen)
    {
        return system_nav_button;
    }
    if (screen == logs_screen)
    {
        return logs_nav_button;
    }
    return NULL;
}

static void set_active_nav_button(lv_obj_t *btn)
{
    if (active_nav_button == btn)
    {
        return;
    }

    if (active_nav_button)
    {
        lv_obj_clear_state(active_nav_button, LV_STATE_CHECKED);
    }

    active_nav_button = btn;

    if (active_nav_button)
    {
        lv_obj_add_state(active_nav_button, LV_STATE_CHECKED);
    }
}

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
        set_active_nav_button(get_nav_button_for_screen(target));
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
        lv_obj_add_style(btn, lv_theme_custom_style_button_active(), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_add_event_cb(btn, nav_button_event_cb, LV_EVENT_CLICKED, buttons[i].target);

        lv_obj_t *label = lv_label_create(btn);
        lv_obj_add_style(label, lv_theme_custom_style_label(), LV_PART_MAIN);
        lv_label_set_text(label, buttons[i].label);
        lv_obj_center(label);

        if (buttons[i].target == dashboard_screen && screen == dashboard_screen)
        {
            dashboard_nav_button = btn;
        }
        else if (buttons[i].target == system_screen && screen == system_screen)
        {
            system_nav_button = btn;
        }
        else if (buttons[i].target == logs_screen && screen == logs_screen)
        {
            logs_nav_button = btn;
        }
    }
}

void ui_manager_set_degraded(bool degraded)
{
    s_degraded = degraded;

    if (s_ui_ready)
    {
        update_degraded_alerts();
    }
    else if (s_degraded)
    {
        ESP_LOGW(TAG, "Mode dégradé activé avant l'initialisation de l'UI");
    }
}

esp_err_t ui_manager_init(void)
{
    if (s_ui_ready)
    {
        ESP_LOGW(TAG, "UI already initialized; skipping duplicate init");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "ui_manager_init: default disp=%p, degraded=%d", (void *)lv_disp_get_default(), s_degraded);

    if (lv_disp_get_default() == NULL)
    {
        ESP_LOGE(TAG, "Default LVGL display missing; UI init aborted (degraded mode)");
        return ESP_ERR_INVALID_STATE;
    }

    lv_theme_custom_init();

    dashboard_screen = dashboard_create();
    system_screen = system_panel_create();
    logs_screen = logs_panel_create();

    if (!dashboard_screen || !system_screen || !logs_screen)
    {
        ESP_LOGE(TAG, "Failed to create one or more screens (dashboard/system/logs)");
        return ESP_ERR_NO_MEM;
    }

    create_navbar(dashboard_screen);
    create_navbar(system_screen);
    create_navbar(logs_screen);

    dashboard_alert = create_degraded_alert(dashboard_screen);
    system_alert = create_degraded_alert(system_screen);
    logs_alert = create_degraded_alert(logs_screen);

    s_ui_ready = true;
    update_degraded_alerts();

    set_active_nav_button(dashboard_nav_button);

    lv_scr_load(dashboard_screen);

    logs_panel_add_log("UI initialisée");
    ESP_LOGI(TAG, "Interface LVGL prête");

    return ESP_OK;
}
