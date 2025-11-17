#include "ui.h"
#include "app_hw.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui";

// Objets globaux
lv_obj_t *ui_SplashScreen = NULL;
lv_obj_t *ui_HomeScreen = NULL;
lv_obj_t *ui_StatusBar = NULL;
lv_obj_t *ui_MenuDrawer = NULL;
lv_obj_t *ui_NetworkScreen = NULL;
lv_obj_t *ui_SystemScreen = NULL;
lv_obj_t *ui_ClimateScreen = NULL;
lv_obj_t *ui_ProfilesScreen = NULL;
lv_obj_t *ui_CommScreen = NULL;
lv_obj_t *ui_StorageScreen = NULL;
lv_obj_t *ui_DiagnosticsScreen = NULL;
lv_obj_t *ui_TouchTestScreen = NULL;
lv_obj_t *ui_AboutScreen = NULL;
lv_obj_t *ui_PinLockScreen = NULL;
lv_obj_t *ui_OTAPlaceholderScreen = NULL;

lv_obj_t *ui_kbAzerty = NULL;
lv_obj_t *ui_taSsid = NULL;
lv_obj_t *ui_taPwd = NULL;
lv_obj_t *ui_lblStatusWifi = NULL;
lv_obj_t *ui_lblStatusSd = NULL;
lv_obj_t *ui_lblStatusClock = NULL;
lv_obj_t *ui_lblStatusBacklight = NULL;
lv_obj_t *ui_lblStatusAlert = NULL;

static lv_obj_t *ui_tabviewHome = NULL;
static lv_obj_t *ui_chartCpu = NULL;
static lv_obj_t *ui_listFrames = NULL;
static lv_obj_t *ui_listFiles = NULL;
static lv_obj_t *ui_listLogs = NULL;
static lv_obj_t *ui_lblTouchCoords = NULL;
static lv_obj_t *ui_taPin = NULL;
static lv_obj_t *ui_lblSdStatus = NULL;
static lv_obj_t *ui_barSdUsage = NULL;
static lv_obj_t *ui_sliderBacklight = NULL;
static lv_obj_t *ui_ddCanBaud = NULL;
static lv_obj_t *ui_ddRs485Baud = NULL;
static lv_obj_t *ui_swBle = NULL;
static lv_obj_t *ui_labelLang = NULL;

static app_ui_state_t s_ui_state = {0};

// Traductions minimales FR/EN
static const char *lang_table[][8] = {
    {"Accueil", "Réseau", "Système", "Climat", "Profils", "Comm", "Stockage", "Diag"},
    {"Home", "Network", "System", "Climate", "Profiles", "Comm", "Storage", "Diag"}
};

// Clavier AZERTY étendu (minuscules)
static const char *kb_map_azerty[] = {
    "1","2","3","4","5","6","7","8","9","0","'","^","\n",
    "a","z","e","r","t","y","u","i","o","p","è","$","\n",
    "q","s","d","f","g","h","j","k","l","m","à","ç","\n",
    "w","x","c","v","b","n",",",";","?",".",LV_SYMBOL_BACKSPACE,"\n",
    LV_SYMBOL_CLOSE," ",LV_SYMBOL_OK,""
};

static const lv_btnmatrix_ctrl_t kb_ctrl_azerty[] = {
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NEW_LINE,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NEW_LINE,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NEW_LINE,
    LV_BTNMATRIX_CTRL_CHECKABLE, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_CHECKABLE, LV_BTNMATRIX_CTRL_CHECKABLE
};

static void create_shared_styles(void) {
    // Palette et styles mutualisés minimalistes pour limiter l'empreinte RAM
    static lv_style_t style_btn_primary;
    static lv_style_t style_btn_secondary;
    static lv_style_t style_panel_card;
    static lv_style_t style_label_value;

    lv_style_init(&style_btn_primary);
    lv_style_set_radius(&style_btn_primary, 8);
    lv_style_set_bg_color(&style_btn_primary, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_text_color(&style_btn_primary, lv_color_white());
    lv_style_set_pad_all(&style_btn_primary, 12);

    lv_style_init(&style_btn_secondary);
    lv_style_set_radius(&style_btn_secondary, 8);
    lv_style_set_bg_color(&style_btn_secondary, lv_color_hex(0x162235));
    lv_style_set_border_color(&style_btn_secondary, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_border_width(&style_btn_secondary, 1);
    lv_style_set_text_color(&style_btn_secondary, lv_color_hex(0xE0E6F0));
    lv_style_set_pad_all(&style_btn_secondary, 12);

    lv_style_init(&style_panel_card);
    lv_style_set_radius(&style_panel_card, 12);
    lv_style_set_bg_color(&style_panel_card, lv_color_hex(0x1E2F45));
    lv_style_set_pad_all(&style_panel_card, 12);
    lv_style_set_shadow_width(&style_panel_card, 8);
    lv_style_set_shadow_color(&style_panel_card, lv_color_hex(0x0B1526));

    lv_style_init(&style_label_value);
    lv_style_set_text_color(&style_label_value, lv_color_hex(0xE0E6F0));
    lv_style_set_text_font(&style_label_value, &lv_font_montserrat_20);

    // Appliquer un thème global simple pour rappel SquareLine (couleurs sombres)
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0B1526), 0);
    LV_UNUSED(style_btn_primary);
    LV_UNUSED(style_btn_secondary);
    LV_UNUSED(style_panel_card);
    LV_UNUSED(style_label_value);
}

static lv_obj_t *create_status_chip(lv_obj_t *parent, const char *icon, lv_obj_t **label_out) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x162235), 0);
    lv_obj_t *lbl_icon = lv_label_create(cont);
    lv_label_set_text(lbl_icon, icon);
    lv_obj_t *lbl_value = lv_label_create(cont);
    lv_label_set_text(lbl_value, "-");
    if (label_out) {
        *label_out = lbl_value;
    }
    return cont;
}

static void create_status_bar(void) {
    if (ui_StatusBar) return;
    ui_StatusBar = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ui_StatusBar, lv_pct(100), 40);
    lv_obj_clear_flag(ui_StatusBar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ui_StatusBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(ui_StatusBar, lv_color_hex(0x0B1526), 0);
    lv_obj_set_style_pad_all(ui_StatusBar, 6, 0);
    lv_obj_set_style_pad_gap(ui_StatusBar, 8, 0);
    lv_obj_align(ui_StatusBar, LV_ALIGN_TOP_MID, 0, 0);

    create_status_chip(ui_StatusBar, LV_SYMBOL_WIFI, &ui_lblStatusWifi);
    create_status_chip(ui_StatusBar, LV_SYMBOL_SD_CARD, &ui_lblStatusSd);
    create_status_chip(ui_StatusBar, LV_SYMBOL_SETTINGS, &ui_lblStatusBacklight);
    create_status_chip(ui_StatusBar, LV_SYMBOL_WARNING, &ui_lblStatusAlert);
    create_status_chip(ui_StatusBar, LV_SYMBOL_CLOCK, &ui_lblStatusClock);
}

static lv_obj_t *create_menu_button(lv_obj_t *parent, const char *txt) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_width(btn, lv_pct(90));
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_center(lbl);
    return btn;
}

static void nav_to_screen(lv_obj_t *scr) {
    if (scr) {
        lv_scr_load(scr);
    }
}

static void create_menu_drawer(lv_obj_t *parent) {
    ui_MenuDrawer = lv_obj_create(parent);
    lv_obj_set_size(ui_MenuDrawer, 220, lv_pct(100));
    lv_obj_align(ui_MenuDrawer, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(ui_MenuDrawer, lv_color_hex(0x162235), 0);
    lv_obj_set_flex_flow(ui_MenuDrawer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(ui_MenuDrawer, 10, 0);
    lv_obj_set_style_pad_gap(ui_MenuDrawer, 6, 0);

    static const char *menu_items[] = {"Accueil", "Réseau", "Système", "Climat", "Profils", "Comm", "Stockage", "Diag", "Tactile", "À propos", "OTA", NULL};
    for (int i = 0; menu_items[i]; ++i) {
        lv_obj_t *btn = create_menu_button(ui_MenuDrawer, menu_items[i]);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(btn, ui_event_tab_swipe, LV_EVENT_CLICKED, (void *)menu_items[i]);
    }
}

static lv_obj_t *create_list_card(lv_obj_t *parent, const char *title) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(45), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    return card;
}

static void splash_timer_cb(lv_timer_t *t) {
    LV_UNUSED(t);
    ui_show_home();
}

static void create_splash_screen(void) {
    ui_SplashScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SplashScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_SplashScreen, lv_color_hex(0x0B1526), 0);
    lv_obj_t *col = lv_obj_create(ui_SplashScreen);
    lv_obj_center(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(col, 12, 0);
    lv_obj_t *logo = lv_label_create(col);
    lv_label_set_text(logo, "Terrarium Controller");
    lv_obj_t *bar = lv_bar_create(col);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 15, LV_ANIM_OFF);
    lv_timer_t *tmr = lv_timer_create(splash_timer_cb, 1200, NULL);
    LV_UNUSED(tmr);
}

static void create_home_screen(void) {
    ui_HomeScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_HomeScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_HomeScreen, lv_color_hex(0x0B1526), 0);

    lv_obj_t *content = lv_obj_create(ui_HomeScreen);
    lv_obj_set_size(content, lv_pct(100), lv_pct(100));
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *row_cards = lv_obj_create(content);
    lv_obj_set_flex_flow(row_cards, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row_cards, 10, 0);
    lv_obj_set_width(row_cards, lv_pct(100));
    create_list_card(row_cards, "Temp/Hygro");
    create_list_card(row_cards, "CO2/UVB");

    lv_obj_t *row_actions = lv_obj_create(content);
    lv_obj_set_width(row_actions, lv_pct(100));
    lv_obj_set_flex_flow(row_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row_actions, 8, 0);
    lv_obj_t *btnClimate = create_menu_button(row_actions, "Détails Climat");
    lv_obj_add_event_cb(btnClimate, ui_event_tab_swipe, LV_EVENT_CLICKED, (void *)"Climat");
    lv_obj_t *btnProfiles = create_menu_button(row_actions, "Profils");
    lv_obj_add_event_cb(btnProfiles, ui_event_tab_swipe, LV_EVENT_CLICKED, (void *)"Profils");
    lv_obj_t *btnDiag = create_menu_button(row_actions, "Diag");
    lv_obj_add_event_cb(btnDiag, ui_event_tab_swipe, LV_EVENT_CLICKED, (void *)"Diag");

    ui_tabviewHome = lv_tabview_create(content, LV_DIR_TOP, 35);
    lv_obj_t *tab1 = lv_tabview_add_tab(ui_tabviewHome, "Graphes");
    lv_obj_t *tab2 = lv_tabview_add_tab(ui_tabviewHome, "Logs");
    lv_obj_t *chart = lv_chart_create(tab1);
    lv_chart_set_point_count(chart, 20);
    lv_obj_set_size(chart, lv_pct(100), 160);
    lv_obj_center(chart);
    ui_listLogs = lv_list_create(tab2);
    lv_obj_set_size(ui_listLogs, lv_pct(100), 180);
    lv_obj_add_event_cb(ui_tabviewHome, ui_event_tab_swipe, LV_EVENT_GESTURE, NULL);
}

static void create_network_screen(void) {
    ui_NetworkScreen = lv_obj_create(NULL);
    lv_obj_t *col = lv_obj_create(ui_NetworkScreen);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(col, 10, 0);
    lv_obj_set_style_pad_gap(col, 6, 0);

    lv_obj_t *row = lv_obj_create(col);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_t *btnScan = create_menu_button(row, "Scan");
    lv_obj_add_event_cb(btnScan, ui_event_btnScan, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnConnect = create_menu_button(row, "Connecter");
    lv_obj_add_event_cb(btnConnect, ui_event_btnConnect, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnDisconnect = create_menu_button(row, "Déconnecter");
    lv_obj_add_event_cb(btnDisconnect, ui_event_btnDisconnect, LV_EVENT_CLICKED, NULL);

    ui_taSsid = lv_textarea_create(col);
    lv_textarea_set_placeholder_text(ui_taSsid, "SSID");
    lv_obj_add_event_cb(ui_taSsid, ui_event_taSsid, LV_EVENT_FOCUSED, NULL);
    ui_taPwd = lv_textarea_create(col);
    lv_textarea_set_placeholder_text(ui_taPwd, "Mot de passe");
    lv_textarea_set_password_mode(ui_taPwd, true);
    lv_obj_add_event_cb(ui_taPwd, ui_event_taPwd, LV_EVENT_FOCUSED, NULL);

    ui_swBle = lv_switch_create(col);
    lv_obj_t *lblBle = lv_label_create(col);
    lv_label_set_text(lblBle, "Bluetooth placeholder");
}

static void create_system_screen(void) {
    ui_SystemScreen = lv_obj_create(NULL);
    lv_obj_t *col = lv_obj_create(ui_SystemScreen);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_pad_all(col, 10, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

    ui_sliderBacklight = lv_slider_create(col);
    lv_slider_set_range(ui_sliderBacklight, 0, 100);
    lv_obj_set_width(ui_sliderBacklight, lv_pct(90));
    lv_obj_add_event_cb(ui_sliderBacklight, ui_event_sliderBacklight, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *row_presets = lv_obj_create(col);
    lv_obj_set_width(row_presets, lv_pct(100));
    lv_obj_set_flex_flow(row_presets, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row_presets, 6, 0);
    lv_obj_t *btnDay = create_menu_button(row_presets, "Jour");
    lv_obj_add_event_cb(btnDay, ui_event_btnPresetDay, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnNight = create_menu_button(row_presets, "Nuit");
    lv_obj_add_event_cb(btnNight, ui_event_btnPresetNight, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnIndoor = create_menu_button(row_presets, "Intérieur");
    lv_obj_add_event_cb(btnIndoor, ui_event_btnPresetIndoor, LV_EVENT_CLICKED, NULL);

    ui_labelLang = lv_label_create(col);
    lv_label_set_text(ui_labelLang, "Langue: FR");
}

static void create_climate_screen(void) {
    ui_ClimateScreen = lv_obj_create(NULL);
    lv_obj_t *grid = lv_obj_create(ui_ClimateScreen);
    lv_obj_set_size(grid, lv_pct(100), lv_pct(100));
    lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(grid, 8, 0);
    create_list_card(grid, "Température");
    create_list_card(grid, "Hygrométrie");
    create_list_card(grid, "CO2");
    create_list_card(grid, "UVB");
}

static void create_profiles_screen(void) {
    ui_ProfilesScreen = lv_obj_create(NULL);
    lv_obj_t *col = lv_obj_create(ui_ProfilesScreen);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(col, 6, 0);

    lv_obj_t *list = lv_list_create(col);
    lv_obj_set_height(list, 180);
    lv_list_add_text(list, "Profils");
    lv_list_add_button(list, LV_SYMBOL_OK, "Jour");
    lv_list_add_button(list, LV_SYMBOL_OK, "Nuit");

    lv_obj_t *btnActivate = create_menu_button(col, "Activer profil");
    lv_obj_add_event_cb(btnActivate, ui_event_btnProfileActivate, LV_EVENT_CLICKED, NULL);
}

static void create_comm_screen(void) {
    ui_CommScreen = lv_obj_create(NULL);
    lv_obj_t *col = lv_obj_create(ui_CommScreen);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_pad_all(col, 10, 0);
    lv_obj_set_style_pad_gap(col, 6, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

    ui_ddCanBaud = lv_dropdown_create(col);
    lv_dropdown_set_options_static(ui_ddCanBaud, "250k\n500k\n1M");
    lv_obj_add_event_cb(ui_ddCanBaud, ui_event_ddCanBaud, LV_EVENT_VALUE_CHANGED, NULL);
    ui_ddRs485Baud = lv_dropdown_create(col);
    lv_dropdown_set_options_static(ui_ddRs485Baud, "9600\n19200\n38400\n115200");
    lv_obj_add_event_cb(ui_ddRs485Baud, ui_event_ddRs485Baud, LV_EVENT_VALUE_CHANGED, NULL);

    ui_listFrames = lv_list_create(col);
    lv_obj_set_height(ui_listFrames, 150);

    lv_obj_t *row = lv_obj_create(col);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_t *btnStart = create_menu_button(row, "Start");
    lv_obj_add_event_cb(btnStart, ui_event_btnCommStart, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnClear = create_menu_button(row, "Clear");
    lv_obj_add_event_cb(btnClear, ui_event_btnCommClear, LV_EVENT_CLICKED, NULL);
}

static void create_storage_screen(void) {
    ui_StorageScreen = lv_obj_create(NULL);
    lv_obj_t *col = lv_obj_create(ui_StorageScreen);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_pad_all(col, 10, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    ui_lblSdStatus = lv_label_create(col);
    lv_label_set_text(ui_lblSdStatus, "SD: inconnue");
    ui_barSdUsage = lv_bar_create(col);
    lv_bar_set_range(ui_barSdUsage, 0, 100);
    lv_bar_set_value(ui_barSdUsage, 0, LV_ANIM_OFF);
    ui_listFiles = lv_list_create(col);
    lv_obj_set_height(ui_listFiles, 160);
    lv_obj_t *row = lv_obj_create(col);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_t *btnRefresh = create_menu_button(row, "Refresh");
    lv_obj_add_event_cb(btnRefresh, ui_event_btnSdRefresh, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnUnmount = create_menu_button(row, "Unmount");
    lv_obj_add_event_cb(btnUnmount, ui_event_btnSdUnmount, LV_EVENT_CLICKED, NULL);
}

static void create_diagnostics_screen(void) {
    ui_DiagnosticsScreen = lv_obj_create(NULL);
    lv_obj_t *tab = lv_tabview_create(ui_DiagnosticsScreen, LV_DIR_TOP, 40);
    lv_obj_align(tab, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_t *tabStats = lv_tabview_add_tab(tab, "Stats");
    lv_obj_t *tabLogs = lv_tabview_add_tab(tab, "Logs");

    ui_chartCpu = lv_chart_create(tabStats);
    lv_chart_set_point_count(ui_chartCpu, 10);
    lv_obj_set_size(ui_chartCpu, lv_pct(100), 160);
    lv_obj_center(ui_chartCpu);

    ui_listLogs = lv_list_create(tabLogs);
    lv_obj_set_size(ui_listLogs, lv_pct(100), 180);
}

static void create_touch_screen(void) {
    ui_TouchTestScreen = lv_obj_create(NULL);
    lv_obj_t *col = lv_obj_create(ui_TouchTestScreen);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_pad_all(col, 10, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *canvas = lv_canvas_create(col);
    lv_obj_set_size(canvas, 320, 240);
    lv_obj_add_event_cb(canvas, ui_event_touch_canvas, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(canvas, ui_event_touch_canvas, LV_EVENT_PRESSING, NULL);
    ui_lblTouchCoords = lv_label_create(col);
    lv_label_set_text(ui_lblTouchCoords, "(x,y)");
    lv_obj_t *btnValidate = create_menu_button(col, "Valider test");
    lv_obj_add_event_cb(btnValidate, ui_event_btnTouchValidate, LV_EVENT_CLICKED, NULL);
}

static void create_about_screen(void) {
    ui_AboutScreen = lv_obj_create(NULL);
    lv_obj_t *col = lv_obj_create(ui_AboutScreen);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_pad_all(col, 10, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *lbl = lv_label_create(col);
    lv_label_set_text(lbl, "Contrôleur terrariophile\nFirmware v0.1.0\nLVGL 9.2 / ESP-IDF 6.x");
}

static void create_touch_keyboard(void) {
    ui_kbAzerty = lv_keyboard_create(lv_layer_top());
    lv_obj_add_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_mode(ui_kbAzerty, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_map(ui_kbAzerty, LV_KEYBOARD_MODE_TEXT_LOWER, kb_map_azerty, kb_ctrl_azerty);
    lv_obj_add_event_cb(ui_kbAzerty, ui_event_kb_ready, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_kbAzerty, ui_event_kb_cancel, LV_EVENT_CANCEL, NULL);
}

static void create_pin_lock_screen(void) {
    ui_PinLockScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_PinLockScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_PinLockScreen, lv_color_hex(0x0B1526), 0);
    lv_obj_t *col = lv_obj_create(ui_PinLockScreen);
    lv_obj_center(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(col, 12, 0);
    lv_obj_t *lbl = lv_label_create(col);
    lv_label_set_text(lbl, "Verrouillage - Entrer PIN");
    ui_taPin = lv_textarea_create(col);
    lv_textarea_set_password_mode(ui_taPin, true);
    lv_obj_set_width(ui_taPin, 140);
    lv_obj_add_event_cb(ui_taPin, ui_event_taPwd, LV_EVENT_FOCUSED, NULL);
    lv_obj_t *btn = create_menu_button(col, "Déverrouiller");
    lv_obj_add_event_cb(btn, ui_event_pin_submit, LV_EVENT_CLICKED, NULL);
}

static void create_ota_screen(void) {
    ui_OTAPlaceholderScreen = lv_obj_create(NULL);
    lv_obj_t *col = lv_obj_create(ui_OTAPlaceholderScreen);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_align(col, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_pad_all(col, 12, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *lbl = lv_label_create(col);
    lv_label_set_text(lbl, "OTA/Export logs placeholders");
    lv_obj_t *btn = create_menu_button(col, "Démarrer OTA");
    lv_obj_add_event_cb(btn, ui_event_btn_ota_start, LV_EVENT_CLICKED, NULL);
}

void ui_init(void) {
    create_shared_styles();
    create_status_bar();
    create_splash_screen();
    create_home_screen();
    create_menu_drawer(ui_HomeScreen);
    create_network_screen();
    create_system_screen();
    create_climate_screen();
    create_profiles_screen();
    create_comm_screen();
    create_storage_screen();
    create_diagnostics_screen();
    create_touch_screen();
    create_about_screen();
    create_pin_lock_screen();
    create_ota_screen();
    create_touch_keyboard();

    lv_scr_load(ui_SplashScreen ? ui_SplashScreen : ui_HomeScreen);
    ui_init_custom();
    ui_show_pin_lock();
}

void ui_init_custom(void) {
    ESP_LOGI(TAG, "Custom init: état initial UI");
    app_ui_state_t init_state = {
        .wifi = {.connected = false, .rssi = -120, .ip = "0.0.0.0"},
        .sd_mounted = false,
        .light = {.backlight_level = 50, .night_mode = false},
        .alert_active = false,
    };
    strcpy(init_state.clock, "--:--");
    app_ui_update_status_bar(&init_state);
    lv_slider_set_value(ui_sliderBacklight, init_state.light.backlight_level, LV_ANIM_OFF);
}

void ui_show_home(void) {
    nav_to_screen(ui_HomeScreen);
}

void ui_show_pin_lock(void) {
    nav_to_screen(ui_PinLockScreen);
}

void ui_set_language(uint8_t lang_id) {
    if (lang_id > 1 || !ui_MenuDrawer) return;
    // simple update of labels
    lv_obj_t *child = lv_obj_get_child(ui_MenuDrawer, 0);
    int idx = 0;
    while (child) {
        lv_obj_t *lbl = lv_obj_get_child(child, 0);
        if (lbl && lv_obj_check_type(lbl, &lv_label_class) && idx < 8) {
            lv_label_set_text(lbl, lang_table[lang_id][idx]);
        }
        child = lv_obj_get_child(ui_MenuDrawer, ++idx);
    }
    if (ui_labelLang) {
        lv_label_set_text_fmt(ui_labelLang, "Langue: %s", lang_id == 0 ? "FR" : "EN");
    }
}

void ui_event_taSsid(lv_event_t *e) {
    lv_obj_clear_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_keyboard_set_textarea(ui_kbAzerty, ta);
}

void ui_event_taPwd(lv_event_t *e) {
    lv_obj_clear_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_keyboard_set_textarea(ui_kbAzerty, ta);
}

void ui_event_kb_ready(lv_event_t *e) {
    LV_UNUSED(e);
    lv_obj_add_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
}

void ui_event_kb_cancel(lv_event_t *e) {
    LV_UNUSED(e);
    lv_obj_add_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
}

void ui_event_btnConnect(lv_event_t *e) {
    LV_UNUSED(e);
    const char *ssid = ui_taSsid ? lv_textarea_get_text(ui_taSsid) : "";
    const char *pwd = ui_taPwd ? lv_textarea_get_text(ui_taPwd) : "";
    if (hw_network_connect(ssid, pwd) == ESP_OK) {
        app_ui_wifi_state_t wifi = {.connected = true, .rssi = -50};
        strncpy(wifi.ip, "192.168.1.10", sizeof(wifi.ip));
        app_ui_set_wifi_state(&wifi);
    }
}

void ui_event_btnDisconnect(lv_event_t *e) {
    LV_UNUSED(e);
    hw_network_disconnect();
    app_ui_wifi_state_t wifi = {.connected = false, .rssi = -120};
    strncpy(wifi.ip, "0.0.0.0", sizeof(wifi.ip));
    app_ui_set_wifi_state(&wifi);
}

void ui_event_btnScan(lv_event_t *e) {
    LV_UNUSED(e);
    ESP_LOGI(TAG, "Scan Wi-Fi demandé (stub)");
}

void ui_event_sliderBacklight(lv_event_t *e) {
    int32_t val = lv_slider_get_value(ui_sliderBacklight);
    hw_backlight_set_level((uint8_t)val);
    app_ui_light_state_t light = {.backlight_level = (uint8_t)val, .night_mode = val < 30};
    app_ui_set_light_state(&light);
}

void ui_event_btnPresetDay(lv_event_t *e) {
    LV_UNUSED(e);
    lv_slider_set_value(ui_sliderBacklight, 90, LV_ANIM_ON);
    ui_event_sliderBacklight(NULL);
}

void ui_event_btnPresetNight(lv_event_t *e) {
    LV_UNUSED(e);
    lv_slider_set_value(ui_sliderBacklight, 20, LV_ANIM_ON);
    ui_event_sliderBacklight(NULL);
}

void ui_event_btnPresetIndoor(lv_event_t *e) {
    LV_UNUSED(e);
    lv_slider_set_value(ui_sliderBacklight, 60, LV_ANIM_ON);
    ui_event_sliderBacklight(NULL);
}

void ui_event_ddCanBaud(lv_event_t *e) {
    LV_UNUSED(e);
    uint32_t baud = 500000;
    hw_comm_set_can_baudrate(baud);
}

void ui_event_ddRs485Baud(lv_event_t *e) {
    LV_UNUSED(e);
    uint32_t baud = 115200;
    hw_comm_set_rs485_baudrate(baud);
}

void ui_event_btnSdRefresh(lv_event_t *e) {
    LV_UNUSED(e);
    hw_sdcard_refresh_status();
}

void ui_event_btnSdUnmount(lv_event_t *e) {
    LV_UNUSED(e);
    ESP_LOGI(TAG, "Unmount SD placeholder");
}

void ui_event_btnCommStart(lv_event_t *e) {
    LV_UNUSED(e);
    lv_list_add_text(ui_listFrames, "Start capture");
}

void ui_event_btnCommClear(lv_event_t *e) {
    LV_UNUSED(e);
    lv_obj_clean(ui_listFrames);
}

void ui_event_btnProfileActivate(lv_event_t *e) {
    LV_UNUSED(e);
    lv_list_add_text(ui_listLogs, "Profil activé");
}

void ui_event_btnTouchValidate(lv_event_t *e) {
    LV_UNUSED(e);
    hw_touch_stop_test();
}

void ui_event_touch_canvas(lv_event_t *e) {
    lv_point_t p;
    lv_event_get_point(e, &p);
    if (ui_lblTouchCoords) {
        lv_label_set_text_fmt(ui_lblTouchCoords, "(%d,%d)", p.x, p.y);
    }
    if (e->code == LV_EVENT_PRESSED) {
        hw_touch_start_test();
    }
}

void ui_event_tab_swipe(lv_event_t *e) {
    if (e->code == LV_EVENT_CLICKED) {
        const char *tag = (const char *)lv_event_get_user_data(e);
        if (!tag) return;
        if (strcmp(tag, "Accueil") == 0) nav_to_screen(ui_HomeScreen);
        else if (strcmp(tag, "Réseau") == 0) nav_to_screen(ui_NetworkScreen);
        else if (strcmp(tag, "Système") == 0) nav_to_screen(ui_SystemScreen);
        else if (strcmp(tag, "Climat") == 0) nav_to_screen(ui_ClimateScreen);
        else if (strcmp(tag, "Profils") == 0) nav_to_screen(ui_ProfilesScreen);
        else if (strcmp(tag, "Comm") == 0) nav_to_screen(ui_CommScreen);
        else if (strcmp(tag, "Stockage") == 0) nav_to_screen(ui_StorageScreen);
        else if (strcmp(tag, "Diag") == 0) nav_to_screen(ui_DiagnosticsScreen);
        else if (strcmp(tag, "Tactile") == 0) nav_to_screen(ui_TouchTestScreen);
        else if (strcmp(tag, "À propos") == 0) nav_to_screen(ui_AboutScreen);
        else if (strcmp(tag, "OTA") == 0) nav_to_screen(ui_OTAPlaceholderScreen);
    }
}

void ui_event_pin_submit(lv_event_t *e) {
    LV_UNUSED(e);
    const char *pin = lv_textarea_get_text(ui_taPin);
    if (strcmp(pin, "1234") == 0) {
        ui_show_home();
    } else {
        lv_label_set_text(ui_lblStatusAlert, "PIN erroné");
        app_ui_set_alert(true);
    }
}

void ui_event_btn_ota_start(lv_event_t *e) {
    LV_UNUSED(e);
    lv_list_add_text(ui_listLogs, "OTA démarré (placeholder)");
}

void ui_event_btn_alert_ack(lv_event_t *e) {
    LV_UNUSED(e);
    app_ui_set_alert(false);
}

static void update_status_labels(void) {
    if (ui_lblStatusWifi) {
        lv_label_set_text_fmt(ui_lblStatusWifi, "%s %s", s_ui_state.wifi.connected ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE, s_ui_state.wifi.ip);
    }
    if (ui_lblStatusSd) {
        lv_label_set_text(ui_lblStatusSd, s_ui_state.sd_mounted ? "SD OK" : "SD --");
    }
    if (ui_lblStatusBacklight) {
        lv_label_set_text_fmt(ui_lblStatusBacklight, "%u%%", s_ui_state.light.backlight_level);
    }
    if (ui_lblStatusAlert) {
        lv_label_set_text(ui_lblStatusAlert, s_ui_state.alert_active ? "ALERT" : "-");
    }
    if (ui_lblStatusClock) {
        lv_label_set_text(ui_lblStatusClock, s_ui_state.clock);
    }
    if (ui_lblSdStatus) {
        lv_label_set_text(ui_lblSdStatus, s_ui_state.sd_mounted ? "SD montée" : "SD absente");
    }
}

void app_ui_update_status_bar(const app_ui_state_t *state) {
    if (!state) return;
    s_ui_state = *state;
    update_status_labels();
}

void app_ui_set_wifi_state(const app_ui_wifi_state_t *wifi) {
    if (!wifi) return;
    s_ui_state.wifi = *wifi;
    update_status_labels();
}

void app_ui_set_sd_state(bool mounted) {
    s_ui_state.sd_mounted = mounted;
    update_status_labels();
}

void app_ui_set_light_state(const app_ui_light_state_t *light) {
    if (!light) return;
    s_ui_state.light = *light;
    update_status_labels();
}

void app_ui_set_alert(bool active) {
    s_ui_state.alert_active = active;
    update_status_labels();
}

void app_ui_set_clock(const char *clock_text) {
    if (clock_text) {
        strncpy(s_ui_state.clock, clock_text, sizeof(s_ui_state.clock));
        s_ui_state.clock[sizeof(s_ui_state.clock) - 1] = '\0';
        update_status_labels();
    }
}
