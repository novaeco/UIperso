#include "ui.h"
#include "app_hw.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "ui_init";

lv_obj_t *ui_ScreenMain = NULL;
lv_obj_t *ui_kbAzerty = NULL;
lv_obj_t *ui_taSsid = NULL;
lv_obj_t *ui_taPwd = NULL;

static const char *kb_map_azerty[] = {
    "a","z","e","r","t","y","u","i","o","p","\n",
    "q","s","d","f","g","h","j","k","l","m","\n",
    "w","x","c","v","b","n",LV_SYMBOL_BACKSPACE,"\n",
    LV_SYMBOL_CLOSE, " ", LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t kb_ctrl_azerty[] = {
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NEW_LINE,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NEW_LINE,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
    LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_CHECKABLE, LV_BTNMATRIX_CTRL_NEW_LINE,
    LV_BTNMATRIX_CTRL_CHECKABLE, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_CHECKABLE, LV_BTNMATRIX_CTRL_CHECKABLE
};

static void create_keyboard(lv_obj_t *parent) {
    ui_kbAzerty = lv_keyboard_create(parent);
    lv_obj_add_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_mode(ui_kbAzerty, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_map(ui_kbAzerty, LV_KEYBOARD_MODE_TEXT_LOWER, kb_map_azerty, kb_ctrl_azerty);
    lv_obj_add_event_cb(ui_kbAzerty, ui_event_kbReady, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_kbAzerty, ui_event_kbCancel, LV_EVENT_CANCEL, NULL);
}

static void create_main_screen(void) {
    ui_ScreenMain = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenMain, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *col = lv_obj_create(ui_ScreenMain);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(col, 12, 0);

    lv_obj_t *lbl = lv_label_create(col);
    lv_label_set_text(lbl, "UI Terrariophile - placeholders SquareLine");

    ui_taSsid = lv_textarea_create(col);
    lv_textarea_set_placeholder_text(ui_taSsid, "SSID");
    lv_obj_add_event_cb(ui_taSsid, ui_event_taSsid, LV_EVENT_FOCUSED, NULL);

    ui_taPwd = lv_textarea_create(col);
    lv_textarea_set_placeholder_text(ui_taPwd, "Mot de passe");
    lv_textarea_set_password_mode(ui_taPwd, true);
    lv_obj_add_event_cb(ui_taPwd, ui_event_taPwd, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *btn_connect = lv_button_create(col);
    lv_obj_set_width(btn_connect, lv_pct(50));
    lv_obj_add_event_cb(btn_connect, ui_event_btnConnect, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_btn = lv_label_create(btn_connect);
    lv_label_set_text(lbl_btn, "Connecter Wi-Fi");
    lv_obj_center(lbl_btn);

    create_keyboard(ui_ScreenMain);
}

void ui_init(void) {
    create_main_screen();
    lv_scr_load(ui_ScreenMain);
    ui_init_custom();
}

void ui_init_custom(void) {
    ESP_LOGI(TAG, "Custom UI init (styles, themes, bindings)");
    // TODO: appliquer styles mutualisés, themes, binding status bar
}

void ui_event_taSsid(lv_event_t *e) {
    LV_UNUSED(e);
    lv_obj_clear_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(ui_kbAzerty, ui_taSsid);
}

void ui_event_taPwd(lv_event_t *e) {
    LV_UNUSED(e);
    lv_obj_clear_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(ui_kbAzerty, ui_taPwd);
}

void ui_event_kbReady(lv_event_t *e) {
    LV_UNUSED(e);
    lv_obj_add_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
}

void ui_event_kbCancel(lv_event_t *e) {
    LV_UNUSED(e);
    lv_obj_add_flag(ui_kbAzerty, LV_OBJ_FLAG_HIDDEN);
}

void ui_event_btnConnect(lv_event_t *e) {
    LV_UNUSED(e);
    const char *ssid = lv_textarea_get_text(ui_taSsid);
    const char *pwd = lv_textarea_get_text(ui_taPwd);
    hw_network_connect(ssid, pwd);
    app_ui_update_status_bar();
}

void app_ui_update_status_bar(void) {
    // TODO: mettre à jour icônes/labels status bar
}
