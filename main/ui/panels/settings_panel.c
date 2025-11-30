#include "settings_panel.h"

#include <string.h>

static const ui_theme_styles_t *s_theme = NULL;
static const system_status_t *s_status = NULL;
static wifi_connect_cb_t s_connect_cb = NULL;
static reptile_wifi_credentials_t *s_creds = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_ssid_ta = NULL;
static lv_obj_t *s_pass_ta = NULL;

static void connect_event_cb(lv_event_t *e)
{
    (void)e;
    if (!s_creds || !s_connect_cb)
    {
        return;
    }
    const char *ssid = lv_textarea_get_text(s_ssid_ta);
    const char *pass = lv_textarea_get_text(s_pass_ta);
    strlcpy(s_creds->ssid, ssid, sizeof(s_creds->ssid));
    strlcpy(s_creds->password, pass, sizeof(s_creds->password));
    s_connect_cb(s_creds);
}

static lv_obj_t *build_row(lv_obj_t *parent, const char *label_text, lv_obj_t **out_ta, bool password)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(row, 6, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(row);
    lv_obj_add_style(label, &s_theme->value, LV_PART_MAIN);
    lv_label_set_text(label, label_text);

    lv_obj_t *ta = lv_textarea_create(row);
    lv_obj_set_width(ta, lv_pct(100));
    lv_textarea_set_one_line(ta, true);
    if (password)
    {
        lv_textarea_set_password_mode(ta, true);
    }

    if (out_ta)
    {
        *out_ta = ta;
    }
    return row;
}

lv_obj_t *settings_panel_create(lv_obj_t *parent,
                                const ui_theme_styles_t *theme,
                                const system_status_t *status_ref,
                                wifi_connect_cb_t cb,
                                reptile_wifi_credentials_t *creds)
{
    s_theme = theme;
    s_status = status_ref;
    s_connect_cb = cb;
    s_creds = creds;

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &s_theme->bg, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);

    lv_obj_t *header = lv_label_create(panel);
    lv_obj_add_style(header, &s_theme->title, LV_PART_MAIN);
    lv_label_set_text(header, "Paramètres");

    build_row(panel, "SSID Wi-Fi", &s_ssid_ta, false);
    build_row(panel, "Mot de passe", &s_pass_ta, true);

    if (s_creds)
    {
        lv_textarea_set_text(s_ssid_ta, s_creds->ssid);
        lv_textarea_set_text(s_pass_ta, s_creds->password);
    }

    lv_obj_t *connect_btn = lv_btn_create(panel);
    lv_obj_add_style(connect_btn, &s_theme->badge, LV_PART_MAIN);
    lv_obj_set_width(connect_btn, lv_pct(100));
    lv_obj_add_event_cb(connect_btn, connect_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *connect_lbl = lv_label_create(connect_btn);
    lv_label_set_text(connect_lbl, LV_SYMBOL_WIFI " Connecter");
    lv_obj_center(connect_lbl);

    s_status_label = lv_label_create(panel);
    lv_obj_add_style(s_status_label, &s_theme->subtle, LV_PART_MAIN);

    settings_panel_update(status_ref, s_creds);
    return panel;
}

void settings_panel_update(const system_status_t *status_ref, const reptile_wifi_credentials_t *creds)
{
    const system_status_t *status = status_ref ? status_ref : s_status;
    if (!status)
    {
        return;
    }

    const char *wifi_state = status->wifi_connected ? "connecté" : "déconnecté";
    const char *ssid = status->wifi_connected ? status->wifi_ssid : "aucun";
    lv_label_set_text_fmt(s_status_label, "Wi-Fi: %s (%s) | SD: %s", wifi_state, ssid, status->sd_mounted ? "OK" : "absente");

    if (creds)
    {
        lv_textarea_set_text(s_ssid_ta, creds->ssid);
        lv_textarea_set_text(s_pass_ta, creds->password);
    }
}
