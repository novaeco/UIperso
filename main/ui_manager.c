#include "ui_manager.h"

#include <inttypes.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "rgb_lcd.h"

#include "ui/panels/dashboard_panel.h"
#include "ui/panels/reptiles_panel.h"
#include "ui/panels/logs_panel_view.h"
#include "ui/panels/settings_panel.h"
#include "ui/theme.h"

#define PANEL_COUNT 4
#define ROTATION_PERIOD_MS 4500

static const char *TAG = "ui_manager";

typedef struct
{
    const char *name;
    lv_obj_t *root;
    void (*update_fn)(const system_status_t *status_ref, const ui_manager_ctx_t *ctx);
} panel_entry_t;

static ui_theme_styles_t s_theme = {0};
static const system_status_t *s_status_ref = NULL;
static const ui_manager_ctx_t *s_ctx = NULL;
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_panel_container = NULL;
static lv_obj_t *s_nav_buttons[PANEL_COUNT] = {0};
static panel_entry_t s_panels[PANEL_COUNT] = {0};
static uint8_t s_active_index = 0;
static ui_mode_t s_mode = UI_MODE_NORMAL;
static lv_obj_t *s_mode_label = NULL;
static lv_obj_t *s_heartbeat_label = NULL;
static lv_obj_t *s_ready_label = NULL;
static lv_timer_t *s_rotate_timer = NULL;
static const lv_coord_t s_panel_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const lv_coord_t s_panel_row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

static void update_dashboard_panel(const system_status_t *status_ref, const ui_manager_ctx_t *ctx)
{
    dashboard_panel_update(status_ref, ctx->reptiles, ctx->wifi_state);
}

static void update_reptiles_panel(const system_status_t *status_ref, const ui_manager_ctx_t *ctx)
{
    (void)status_ref;
    reptiles_panel_update(ctx->reptiles);
}

static void update_logs_panel(const system_status_t *status_ref, const ui_manager_ctx_t *ctx)
{
    (void)status_ref;
    logs_panel_view_update(ctx->reptiles);
}

static void update_settings_panel(const system_status_t *status_ref, const ui_manager_ctx_t *ctx)
{
    settings_panel_update(status_ref, ctx->wifi_creds);
}

static void set_active_panel(uint8_t index)
{
    if (index >= PANEL_COUNT || s_panels[index].root == NULL)
    {
        return;
    }

    s_active_index = index;
    lv_obj_move_foreground(s_panels[index].root);
    for (uint8_t i = 0; i < PANEL_COUNT; i++)
    {
        if (s_nav_buttons[i])
        {
            if (i == index)
            {
                lv_obj_add_state(s_nav_buttons[i], LV_STATE_CHECKED);
            }
            else
            {
                lv_obj_clear_state(s_nav_buttons[i], LV_STATE_CHECKED);
            }
        }
    }
}

static void nav_event_cb(lv_event_t *e)
{
    uint32_t idx = (uint32_t)lv_event_get_user_data(e);
    set_active_panel((uint8_t)idx);
}

static bool grid_template_valid(const lv_coord_t *dsc)
{
    if (!dsc)
    {
        return false;
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        if (dsc[i] == LV_GRID_TEMPLATE_LAST)
        {
            return true;
        }
    }

    return false;
}

static void update_mode_indicator(void)
{
    if (!s_mode_label)
    {
        return;
    }

    const char *label = "Mode normal";
    if (s_mode == UI_MODE_DEGRADED_TOUCH)
    {
        label = "Mode dégradé : tactile";
    }
    else if (s_mode == UI_MODE_DEGRADED_SD)
    {
        label = "Mode dégradé : microSD";
    }
    lv_label_set_text(s_mode_label, label);
}

static void rotation_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_mode != UI_MODE_DEGRADED_TOUCH)
    {
        return;
    }
    set_active_panel((uint8_t)((s_active_index + 1) % PANEL_COUNT));
}

static void create_navbar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(bar, 8, LV_PART_MAIN);

    static const char *labels[PANEL_COUNT] = {"Dashboard", "Reptiles", "Documents", "Paramètres"};
    for (uint8_t i = 0; i < PANEL_COUNT; i++)
    {
        lv_obj_t *btn = lv_btn_create(bar);
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &s_theme.badge, LV_PART_MAIN);
        lv_obj_add_event_cb(btn, nav_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_set_flex_grow(btn, 1);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_center(lbl);

        s_nav_buttons[i] = btn;
    }
}

esp_err_t ui_manager_init(lv_display_t *disp, const system_status_t *status_ref, const ui_manager_ctx_t *ctx)
{
    if (disp == NULL || status_ref == NULL || ctx == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters: disp=%p status_ref=%p ctx=%p", (void *)disp, (const void *)status_ref, (const void *)ctx);
        return ESP_ERR_INVALID_ARG;
    }

    if (!grid_template_valid(s_panel_col_dsc) || !grid_template_valid(s_panel_row_dsc))
    {
        ESP_LOGE(TAG, "Grid descriptors invalid (col_dsc=%p row_dsc=%p)", (void *)s_panel_col_dsc, (void *)s_panel_row_dsc);
        return ESP_ERR_INVALID_STATE;
    }

    if (lv_disp_get_default() == NULL)
    {
        ESP_LOGE(TAG, "Default display not set before ui_manager_init");
        return ESP_ERR_INVALID_STATE;
    }

    s_status_ref = status_ref;
    s_ctx = ctx;
    ui_theme_init(&s_theme);

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_add_style(s_screen, &s_theme.bg, LV_PART_MAIN);
    lv_obj_set_layout(s_screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_screen, 10, LV_PART_MAIN);

    s_ready_label = lv_label_create(s_screen);
    lv_obj_add_style(s_ready_label, &s_theme.title, LV_PART_MAIN);
    lv_obj_set_width(s_ready_label, lv_pct(100));
    lv_obj_set_style_text_align(s_ready_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(s_ready_label, "UI READY");

    lv_obj_t *header = lv_obj_create(s_screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &s_theme.title, LV_PART_MAIN);
    lv_label_set_text(title, "UI Manager");

    s_mode_label = lv_label_create(header);
    lv_obj_add_style(s_mode_label, &s_theme.subtle, LV_PART_MAIN);

    s_panel_container = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_panel_container);
    lv_obj_set_width(s_panel_container, lv_pct(100));
    lv_obj_set_flex_grow(s_panel_container, 1);
    lv_obj_set_layout(s_panel_container, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(s_panel_container, s_panel_col_dsc, s_panel_row_dsc);

    s_panels[0].name = "dashboard";
    s_panels[0].root = dashboard_panel_create(s_panel_container, &s_theme, s_status_ref, s_ctx->reptiles, s_ctx->wifi_state, s_ctx->storage_mount);
    s_panels[0].update_fn = update_dashboard_panel;

    s_panels[1].name = "reptiles";
    s_panels[1].root = reptiles_panel_create(s_panel_container, &s_theme, s_ctx->reptiles, s_ctx->save_data_cb);
    s_panels[1].update_fn = update_reptiles_panel;

    s_panels[2].name = "logs";
    s_panels[2].root = logs_panel_view_create(s_panel_container, &s_theme, s_ctx->reptiles);
    s_panels[2].update_fn = update_logs_panel;

    s_panels[3].name = "settings";
    s_panels[3].root = settings_panel_create(s_panel_container, &s_theme, s_status_ref, s_ctx->wifi_connect_cb, s_ctx->wifi_creds);
    s_panels[3].update_fn = update_settings_panel;

    bool all_panels_ready = true;

    for (uint8_t i = 0; i < PANEL_COUNT; i++)
    {
        if (s_panels[i].root)
        {
            lv_obj_set_grid_cell(s_panels[i].root, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
        }
        else
        {
            ESP_LOGE(TAG, "Panel '%s' failed to create", s_panels[i].name ? s_panels[i].name : "(unnamed)");
            all_panels_ready = false;
        }
    }

    create_navbar(s_screen);
    s_heartbeat_label = lv_label_create(s_screen);
    lv_obj_add_style(s_heartbeat_label, &s_theme.subtle, LV_PART_MAIN);

    if (!all_panels_ready)
    {
        lv_obj_t *fallback = lv_obj_create(NULL);
        lv_obj_remove_style_all(fallback);
        lv_obj_set_style_bg_color(fallback, lv_color_hex(0x5C0010), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(fallback, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_t *label = lv_label_create(fallback);
        lv_label_set_text(label, "UI indisponible (panel manquant)");
        lv_obj_center(label);
        lv_scr_load(fallback);
        ESP_LOGE(TAG, "UI manager fallback loaded due to missing panels");
        return ESP_FAIL;
    }

    set_active_panel(0);
    update_mode_indicator();
    lv_scr_load(s_screen);
    lv_obj_invalidate(lv_scr_act());

    s_rotate_timer = lv_timer_create(rotation_timer_cb, ROTATION_PERIOD_MS, NULL);
    ESP_LOGI(TAG, "UI canonical init done (panels=%d)", PANEL_COUNT);
    return ESP_OK;
}

void ui_manager_set_mode(ui_mode_t mode)
{
    s_mode = mode;
    update_mode_indicator();
}

void ui_manager_tick_1s(void)
{
    if (!s_status_ref || !s_ctx)
    {
        return;
    }

    for (uint8_t i = 0; i < PANEL_COUNT; i++)
    {
        if (s_panels[i].update_fn)
        {
            s_panels[i].update_fn(s_status_ref, s_ctx);
        }
    }

    if (s_heartbeat_label)
    {
        const uint32_t flush = rgb_lcd_flush_count_get();
        const uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        lv_label_set_text_fmt(s_heartbeat_label, "uptime %" PRIu32 "s | flush %" PRIu32, uptime, flush);
    }

    if (s_ready_label)
    {
        const uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        lv_label_set_text_fmt(s_ready_label, "UI READY | uptime %" PRIu32 " s", uptime);
    }
}

