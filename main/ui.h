#pragma once
#include "lvgl.h"
#include "app_ui.h"

extern lv_obj_t *ui_SplashScreen;
extern lv_obj_t *ui_HomeScreen;
extern lv_obj_t *ui_NetworkScreen;
extern lv_obj_t *ui_StorageScreen;
extern lv_obj_t *ui_CommScreen;
extern lv_obj_t *ui_DiagnosticsScreen;
extern lv_obj_t *ui_SettingsScreen;
extern lv_obj_t *ui_TouchTestScreen;
extern lv_obj_t *ui_AboutScreen;
extern lv_obj_t *ui_StatusBar;
extern lv_obj_t *ui_KbAzerty;
extern lv_obj_t *ui_taFocused;
extern lv_obj_t *ui_lblStatusWifi;
extern lv_obj_t *ui_lblStatusTime;
extern lv_obj_t *ui_lblStatusAlerts;
extern lv_obj_t *ui_lblNetworkRssi;
extern lv_obj_t *ui_lblSdStatus;
extern lv_obj_t *ui_barStorage;
extern lv_obj_t *ui_lblStorage;
extern lv_obj_t *ui_lblCanStatus;
extern lv_obj_t *ui_lblRs485Status;
extern lv_obj_t *ui_lblDiagStats;
extern lv_obj_t *ui_sliderBacklight;
extern lv_obj_t *ui_lblTouchCoords;
extern lv_obj_t *ui_taSsid;
extern lv_obj_t *ui_taPassword;

void ui_init(void);
void ui_build_styles(void);
lv_obj_t *ui_get_screen(ui_screen_id_t id);
lv_obj_t *ui_get_status_bar(ui_screen_id_t id);
void ui_set_status_targets(ui_screen_id_t id);
void ui_keyboard_attach_textarea(lv_obj_t *ta);
void ui_keyboard_detach(void);
