#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

// Écrans principaux
extern lv_obj_t *ui_SplashScreen;
extern lv_obj_t *ui_HomeScreen;
extern lv_obj_t *ui_StatusBar;
extern lv_obj_t *ui_MenuDrawer;
extern lv_obj_t *ui_NetworkScreen;
extern lv_obj_t *ui_SystemScreen;
extern lv_obj_t *ui_ClimateScreen;
extern lv_obj_t *ui_ProfilesScreen;
extern lv_obj_t *ui_CommScreen;
extern lv_obj_t *ui_StorageScreen;
extern lv_obj_t *ui_DiagnosticsScreen;
extern lv_obj_t *ui_TouchTestScreen;
extern lv_obj_t *ui_AboutScreen;
extern lv_obj_t *ui_PinLockScreen;
extern lv_obj_t *ui_OTAPlaceholderScreen;

// Widgets partagés
extern lv_obj_t *ui_kbAzerty;
extern lv_obj_t *ui_taSsid;
extern lv_obj_t *ui_taPwd;
extern lv_obj_t *ui_lblStatusWifi;
extern lv_obj_t *ui_lblStatusSd;
extern lv_obj_t *ui_lblStatusClock;
extern lv_obj_t *ui_lblStatusBacklight;
extern lv_obj_t *ui_lblStatusAlert;

// Structures d'état pour mise à jour de la status bar et des écrans
typedef struct {
    bool connected;
    int32_t rssi;
    char ip[24];
} app_ui_wifi_state_t;

typedef struct {
    uint8_t backlight_level; // 0-100
    bool night_mode;
} app_ui_light_state_t;

typedef struct {
    app_ui_wifi_state_t wifi;
    bool sd_mounted;
    app_ui_light_state_t light;
    bool alert_active;
    char clock[12];
} app_ui_state_t;

void ui_init(void);
void ui_init_custom(void);
void ui_show_home(void);
void ui_show_pin_lock(void);
void ui_set_language(uint8_t lang_id);

// Callbacks UI (SquareLine style)
void ui_event_taSsid(lv_event_t *e);
void ui_event_taPwd(lv_event_t *e);
void ui_event_kb_ready(lv_event_t *e);
void ui_event_kb_cancel(lv_event_t *e);
void ui_event_btnConnect(lv_event_t *e);
void ui_event_btnDisconnect(lv_event_t *e);
void ui_event_btnScan(lv_event_t *e);
void ui_event_sliderBacklight(lv_event_t *e);
void ui_event_btnPresetDay(lv_event_t *e);
void ui_event_btnPresetNight(lv_event_t *e);
void ui_event_btnPresetIndoor(lv_event_t *e);
void ui_event_ddCanBaud(lv_event_t *e);
void ui_event_ddRs485Baud(lv_event_t *e);
void ui_event_btnSdRefresh(lv_event_t *e);
void ui_event_btnSdUnmount(lv_event_t *e);
void ui_event_btnCommStart(lv_event_t *e);
void ui_event_btnCommClear(lv_event_t *e);
void ui_event_btnProfileActivate(lv_event_t *e);
void ui_event_btnTouchValidate(lv_event_t *e);
void ui_event_tab_swipe(lv_event_t *e);
void ui_event_touch_canvas(lv_event_t *e);
void ui_event_pin_submit(lv_event_t *e);
void ui_event_btn_ota_start(lv_event_t *e);
void ui_event_btn_alert_ack(lv_event_t *e);

// Mise à jour status bar
void app_ui_update_status_bar(const app_ui_state_t *state);
void app_ui_set_wifi_state(const app_ui_wifi_state_t *wifi);
void app_ui_set_sd_state(bool mounted);
void app_ui_set_light_state(const app_ui_light_state_t *light);
void app_ui_set_alert(bool active);
void app_ui_set_clock(const char *clock_text);

#endif // UI_H
