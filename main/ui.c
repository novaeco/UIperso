#include "ui.h"
#include "ui_events.h"
#include "ui_helpers.h"
#include "lvgl.h"

#ifndef LVGL_HOR_RES
#define LVGL_HOR_RES 1024
#endif
#ifndef LVGL_VER_RES
#define LVGL_VER_RES 600
#endif

lv_obj_t *ui_SplashScreen;
lv_obj_t *ui_HomeScreen;
lv_obj_t *ui_NetworkScreen;
lv_obj_t *ui_StorageScreen;
lv_obj_t *ui_CommScreen;
lv_obj_t *ui_DiagnosticsScreen;
lv_obj_t *ui_SettingsScreen;
lv_obj_t *ui_TouchTestScreen;
lv_obj_t *ui_AboutScreen;
lv_obj_t *ui_StatusBar;
lv_obj_t *ui_KbAzerty;
lv_obj_t *ui_taFocused;
lv_obj_t *ui_lblStatusWifi;
lv_obj_t *ui_lblStatusTime;
lv_obj_t *ui_lblStatusAlerts;
lv_obj_t *ui_lblNetworkRssi;
lv_obj_t *ui_lblSdStatus;
lv_obj_t *ui_barStorage;
lv_obj_t *ui_lblStorage;
lv_obj_t *ui_lblCanStatus;
lv_obj_t *ui_lblRs485Status;
lv_obj_t *ui_lblDiagStats;
lv_obj_t *ui_sliderBacklight;
lv_obj_t *ui_lblTouchCoords;
lv_obj_t *ui_taSsid;
lv_obj_t *ui_taPassword;

static lv_obj_t *status_bars[UI_SCREEN_ABOUT + 1];
static lv_obj_t *status_wifi_labels[UI_SCREEN_ABOUT + 1];
static lv_obj_t *status_time_labels[UI_SCREEN_ABOUT + 1];
static lv_obj_t *status_alert_labels[UI_SCREEN_ABOUT + 1];

static lv_obj_t *nav_panel_create(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, 240, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_style_pad_gap(panel, 8, 0);

    lv_obj_t *btnHome = lv_btn_create(panel);
    lv_obj_add_style(btnHome, &style_button_primary, 0);
    lv_obj_set_width(btnHome, LV_PCT(100));
    lv_obj_add_event_cb(btnHome, ui_event_btnGoHome, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btnHome), "Accueil");

    lv_obj_t *btnNet = lv_btn_create(panel);
    lv_obj_add_style(btnNet, &style_button_secondary, 0);
    lv_obj_set_width(btnNet, LV_PCT(100));
    lv_obj_add_event_cb(btnNet, ui_event_btnGoNetwork, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btnNet), "Réseau");

    lv_obj_t *btnSto = lv_btn_create(panel);
    lv_obj_add_style(btnSto, &style_button_secondary, 0);
    lv_obj_set_width(btnSto, LV_PCT(100));
    lv_obj_add_event_cb(btnSto, ui_event_btnGoStorage, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btnSto), "Stockage");

    lv_obj_t *btnComm = lv_btn_create(panel);
    lv_obj_add_style(btnComm, &style_button_secondary, 0);
    lv_obj_set_width(btnComm, LV_PCT(100));
    lv_obj_add_event_cb(btnComm, ui_event_btnGoComm, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btnComm), "Communication");

    lv_obj_t *btnDiag = lv_btn_create(panel);
    lv_obj_add_style(btnDiag, &style_button_secondary, 0);
    lv_obj_set_width(btnDiag, LV_PCT(100));
    lv_obj_add_event_cb(btnDiag, ui_event_btnGoDiag, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btnDiag), "Diagnostics");

    lv_obj_t *btnSettings = lv_btn_create(panel);
    lv_obj_add_style(btnSettings, &style_button_secondary, 0);
    lv_obj_set_width(btnSettings, LV_PCT(100));
    lv_obj_add_event_cb(btnSettings, ui_event_btnGoSettings, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btnSettings), "Réglages");

    lv_obj_t *btnTouch = lv_btn_create(panel);
    lv_obj_add_style(btnTouch, &style_button_secondary, 0);
    lv_obj_set_width(btnTouch, LV_PCT(100));
    lv_obj_add_event_cb(btnTouch, ui_event_btnGoTouch, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btnTouch), "Test tactile");

    lv_obj_t *btnAbout = lv_btn_create(panel);
    lv_obj_add_style(btnAbout, &style_button_secondary, 0);
    lv_obj_set_width(btnAbout, LV_PCT(100));
    lv_obj_add_event_cb(btnAbout, ui_event_btnGoAbout, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btnAbout), "À propos");

    return panel;
}

static lv_obj_t *status_bar_create(lv_obj_t *parent, ui_screen_id_t id)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_add_style(bar, &style_status_bar, 0);
    lv_obj_set_size(bar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(bar, 16, 0);
    status_wifi_labels[id] = lv_label_create(bar);
    status_time_labels[id] = lv_label_create(bar);
    status_alert_labels[id] = lv_label_create(bar);
    return bar;
}

static void create_splash(void)
{
    ui_SplashScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SplashScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_SplashScreen, lv_color_hex(0x0B0E14), 0);
    lv_obj_t *label = lv_label_create(ui_SplashScreen);
    lv_obj_add_style(label, &style_title, 0);
    lv_label_set_text(label, "Waveshare ESP32-S3 Touch 7B\nLVGL 9.4 Demo");
    lv_obj_center(label);
}

static void create_home(void)
{
    ui_HomeScreen = lv_obj_create(NULL);
    lv_obj_set_size(ui_HomeScreen, LVGL_HOR_RES, LVGL_VER_RES);
    lv_obj_set_flex_flow(ui_HomeScreen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(ui_HomeScreen, 12, 0);

    status_bars[UI_SCREEN_HOME] = status_bar_create(ui_HomeScreen, UI_SCREEN_HOME);

    lv_obj_t *row = lv_obj_create(ui_HomeScreen);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 12, 0);

    lv_obj_t *nav = nav_panel_create(row);
    (void)nav;

    lv_obj_t *content = lv_obj_create(row);
    lv_obj_add_style(content, &style_panel, 0);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(content, 10, 0);

    lv_obj_t *title = lv_label_create(content);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Dashboard principal");

    lv_obj_t *wifi = lv_label_create(content);
    lv_obj_add_style(wifi, &style_value, 0);
    lv_label_set_text(wifi, "État Wi-Fi");

    lv_obj_t *sd = lv_label_create(content);
    lv_obj_add_style(sd, &style_value, 0);
    lv_label_set_text(sd, "État microSD");

    lv_obj_t *backlight = lv_slider_create(content);
    lv_obj_set_width(backlight, LV_PCT(80));
    lv_slider_set_range(backlight, 0, 100);
    lv_slider_set_value(backlight, 80, LV_ANIM_OFF);
    ui_sliderBacklight = backlight;
    lv_obj_add_event_cb(backlight, ui_event_slider_backlight, LV_EVENT_VALUE_CHANGED, NULL);
}

static void create_network(void)
{
    ui_NetworkScreen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(ui_NetworkScreen, 12, 0);
    lv_obj_set_flex_flow(ui_NetworkScreen, LV_FLEX_FLOW_COLUMN);

    status_bars[UI_SCREEN_NETWORK] = status_bar_create(ui_NetworkScreen, UI_SCREEN_NETWORK);

    lv_obj_t *row = lv_obj_create(ui_NetworkScreen);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_gap(row, 10, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);

    nav_panel_create(row);

    lv_obj_t *panel = lv_obj_create(row);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_gap(panel, 10, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Réseau Wi-Fi");

    lv_obj_t *list = lv_list_create(panel);
    lv_obj_set_size(list, LV_PCT(100), 150);
    lv_list_add_text(list, "SSID disponibles");
    lv_list_add_button(list, NULL, "Réseau 1");
    lv_list_add_button(list, NULL, "Réseau 2");

    ui_taSsid = lv_textarea_create(panel);
    lv_textarea_set_placeholder_text(ui_taSsid, "SSID");
    lv_obj_add_event_cb(ui_taSsid, ui_event_ta_focus_show_kb, LV_EVENT_FOCUSED, NULL);

    ui_taPassword = lv_textarea_create(panel);
    lv_textarea_set_placeholder_text(ui_taPassword, "Mot de passe");
    lv_textarea_set_password_mode(ui_taPassword, true);
    lv_obj_add_event_cb(ui_taPassword, ui_event_ta_focus_show_kb, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *btnRow = lv_obj_create(panel);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(btnRow, 8, 0);
    lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *btnConnect = lv_btn_create(btnRow);
    lv_obj_add_style(btnConnect, &style_button_primary, 0);
    lv_label_set_text(lv_label_create(btnConnect), "Connecter");
    lv_obj_add_event_cb(btnConnect, ui_event_btnConnect, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btnDisconnect = lv_btn_create(btnRow);
    lv_obj_add_style(btnDisconnect, &style_button_secondary, 0);
    lv_label_set_text(lv_label_create(btnDisconnect), "Déconnecter");
    lv_obj_add_event_cb(btnDisconnect, ui_event_btnDisconnect, LV_EVENT_CLICKED, NULL);

    ui_lblNetworkRssi = lv_label_create(panel);
    lv_obj_add_style(ui_lblNetworkRssi, &style_value, 0);
}

static void create_storage(void)
{
    ui_StorageScreen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(ui_StorageScreen, 12, 0);
    lv_obj_set_flex_flow(ui_StorageScreen, LV_FLEX_FLOW_COLUMN);

    status_bars[UI_SCREEN_STORAGE] = status_bar_create(ui_StorageScreen, UI_SCREEN_STORAGE);
    lv_obj_t *row = lv_obj_create(ui_StorageScreen);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 10, 0);

    nav_panel_create(row);

    lv_obj_t *panel = lv_obj_create(row);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(panel, 10, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Stockage microSD");

    ui_lblSdStatus = lv_label_create(panel);
    ui_barStorage = lv_bar_create(panel);
    lv_bar_set_range(ui_barStorage, 0, 100);
    lv_obj_set_width(ui_barStorage, LV_PCT(90));
    ui_lblStorage = lv_label_create(panel);
}

static void create_comm(void)
{
    ui_CommScreen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(ui_CommScreen, 12, 0);
    lv_obj_set_flex_flow(ui_CommScreen, LV_FLEX_FLOW_COLUMN);

    status_bars[UI_SCREEN_COMM] = status_bar_create(ui_CommScreen, UI_SCREEN_COMM);
    lv_obj_t *row = lv_obj_create(ui_CommScreen);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 10, 0);

    nav_panel_create(row);

    lv_obj_t *panel = lv_obj_create(row);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(panel, 10, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "CAN / RS485");

    lv_obj_t *dd_can = lv_dropdown_create(panel);
    lv_dropdown_set_options_static(dd_can, "125000\n250000\n500000\n1000000");
    lv_obj_add_event_cb(dd_can, ui_event_dropdown_can, LV_EVENT_VALUE_CHANGED, NULL);
    ui_lblCanStatus = lv_label_create(panel);

    lv_obj_t *dd_rs = lv_dropdown_create(panel);
    lv_dropdown_set_options_static(dd_rs, "9600\n19200\n38400\n57600\n115200");
    lv_obj_add_event_cb(dd_rs, ui_event_dropdown_rs485, LV_EVENT_VALUE_CHANGED, NULL);
    ui_lblRs485Status = lv_label_create(panel);
}

static void create_diag(void)
{
    ui_DiagnosticsScreen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(ui_DiagnosticsScreen, 12, 0);
    lv_obj_set_flex_flow(ui_DiagnosticsScreen, LV_FLEX_FLOW_COLUMN);

    status_bars[UI_SCREEN_DIAGNOSTICS] = status_bar_create(ui_DiagnosticsScreen, UI_SCREEN_DIAGNOSTICS);
    lv_obj_t *row = lv_obj_create(ui_DiagnosticsScreen);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 10, 0);

    nav_panel_create(row);

    lv_obj_t *panel = lv_obj_create(row);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(panel, 10, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Diagnostics système");

    ui_lblDiagStats = lv_label_create(panel);
}

static void create_settings(void)
{
    ui_SettingsScreen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(ui_SettingsScreen, 12, 0);
    lv_obj_set_flex_flow(ui_SettingsScreen, LV_FLEX_FLOW_COLUMN);

    status_bars[UI_SCREEN_SETTINGS] = status_bar_create(ui_SettingsScreen, UI_SCREEN_SETTINGS);
    lv_obj_t *row = lv_obj_create(ui_SettingsScreen);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 10, 0);

    nav_panel_create(row);

    lv_obj_t *panel = lv_obj_create(row);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(panel, 10, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Réglages & backlight");

    ui_sliderBacklight = lv_slider_create(panel);
    lv_slider_set_range(ui_sliderBacklight, 0, 100);
    lv_slider_set_value(ui_sliderBacklight, 80, LV_ANIM_OFF);
    lv_obj_set_width(ui_sliderBacklight, LV_PCT(80));
    lv_obj_add_event_cb(ui_sliderBacklight, ui_event_slider_backlight, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *btnRow = lv_obj_create(panel);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(btnRow, 8, 0);
    lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *btnIndoor = lv_btn_create(btnRow);
    lv_obj_add_style(btnIndoor, &style_button_secondary, 0);
    lv_label_set_text(lv_label_create(btnIndoor), "Intérieur");
    lv_obj_add_event_cb(btnIndoor, ui_event_btn_profile_indoor, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btnDark = lv_btn_create(btnRow);
    lv_obj_add_style(btnDark, &style_button_secondary, 0);
    lv_label_set_text(lv_label_create(btnDark), "Pièce sombre");
    lv_obj_add_event_cb(btnDark, ui_event_btn_profile_dark, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btnNight = lv_btn_create(btnRow);
    lv_obj_add_style(btnNight, &style_button_secondary, 0);
    lv_label_set_text(lv_label_create(btnNight), "Nuit");
    lv_obj_add_event_cb(btnNight, ui_event_btn_profile_night, LV_EVENT_CLICKED, NULL);
}

static void create_touch(void)
{
    ui_TouchTestScreen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(ui_TouchTestScreen, 12, 0);
    lv_obj_set_flex_flow(ui_TouchTestScreen, LV_FLEX_FLOW_COLUMN);

    status_bars[UI_SCREEN_TOUCH_TEST] = status_bar_create(ui_TouchTestScreen, UI_SCREEN_TOUCH_TEST);
    lv_obj_t *row = lv_obj_create(ui_TouchTestScreen);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 10, 0);

    nav_panel_create(row);

    lv_obj_t *panel = lv_obj_create(row);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(panel, 10, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Test tactile capacitif");

    ui_lblTouchCoords = lv_label_create(panel);
    lv_label_set_text(ui_lblTouchCoords, "Touch: --");

    lv_obj_t *btn = lv_btn_create(panel);
    lv_obj_add_style(btn, &style_button_primary, 0);
    lv_label_set_text(lv_label_create(btn), "Lire coordonnées");
    lv_obj_add_event_cb(btn, ui_event_btn_touch_record, LV_EVENT_CLICKED, NULL);
}

static void create_about(void)
{
    ui_AboutScreen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(ui_AboutScreen, 12, 0);
    lv_obj_set_flex_flow(ui_AboutScreen, LV_FLEX_FLOW_COLUMN);

    status_bars[UI_SCREEN_ABOUT] = status_bar_create(ui_AboutScreen, UI_SCREEN_ABOUT);
    lv_obj_t *row = lv_obj_create(ui_AboutScreen);
    lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 10, 0);

    nav_panel_create(row);

    lv_obj_t *panel = lv_obj_create(row);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(panel, 10, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "À propos / firmware");

    lv_obj_t *label = lv_label_create(panel);
    lv_label_set_text(label, "Firmware ESP-IDF 6.1 + LVGL 9.4\nCarte Waveshare ESP32-S3 Touch LCD 7B");
}

static void create_keyboard(void)
{
    static const char *azerty_map[] = {
        "a", "z", "e", "r", "t", "y", "u", "i", "o", "p", LV_SYMBOL_BACKSPACE, "", "",
        "q", "s", "d", "f", "g", "h", "j", "k", "l", "m", "ù", "", "",
        "w", "x", "c", "v", "b", "n", ",", ";", ":", "?", "!", "é", "è",
        LV_SYMBOL_OK, " ", LV_SYMBOL_CLOSE, "à", "ç", NULL
    };

    ui_KbAzerty = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(ui_KbAzerty, LV_PCT(100), 200);
    lv_obj_add_flag(ui_KbAzerty, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_map(ui_KbAzerty, LV_KEYBOARD_MODE_USER_1, azerty_map, NULL);
    lv_keyboard_set_mode(ui_KbAzerty, LV_KEYBOARD_MODE_USER_1);
    lv_obj_add_event_cb(ui_KbAzerty, ui_event_keyboard_ready, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_KbAzerty, ui_event_keyboard_cancel, LV_EVENT_CANCEL, NULL);
}

lv_obj_t *ui_get_screen(ui_screen_id_t id)
{
    switch (id) {
    case UI_SCREEN_SPLASH: return ui_SplashScreen;
    case UI_SCREEN_HOME: return ui_HomeScreen;
    case UI_SCREEN_NETWORK: return ui_NetworkScreen;
    case UI_SCREEN_STORAGE: return ui_StorageScreen;
    case UI_SCREEN_COMM: return ui_CommScreen;
    case UI_SCREEN_DIAGNOSTICS: return ui_DiagnosticsScreen;
    case UI_SCREEN_SETTINGS: return ui_SettingsScreen;
    case UI_SCREEN_TOUCH_TEST: return ui_TouchTestScreen;
    case UI_SCREEN_ABOUT: return ui_AboutScreen;
    default: return NULL;
    }
}

lv_obj_t *ui_get_status_bar(ui_screen_id_t id)
{
    if (id <= UI_SCREEN_ABOUT) {
        return status_bars[id];
    }
    return NULL;
}

void ui_set_status_targets(ui_screen_id_t id)
{
    if (id <= UI_SCREEN_ABOUT) {
        ui_StatusBar = status_bars[id];
        ui_lblStatusWifi = status_wifi_labels[id];
        ui_lblStatusTime = status_time_labels[id];
        ui_lblStatusAlerts = status_alert_labels[id];
    }
}

void ui_keyboard_attach_textarea(lv_obj_t *ta)
{
    lv_keyboard_set_textarea(ui_KbAzerty, ta);
}

void ui_keyboard_detach(void)
{
    lv_keyboard_set_textarea(ui_KbAzerty, NULL);
}

void ui_init(void)
{
    ui_style_init();
    create_splash();
    create_home();
    create_network();
    create_storage();
    create_comm();
    create_diag();
    create_settings();
    create_touch();
    create_about();
    create_keyboard();
}
