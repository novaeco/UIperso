#include "ui_events.h"
#include "app_ui.h"
#include "app_hw.h"
#include "ui.h"
#include "esp_log.h"

static const char *TAG = "ui_events";

void ui_event_btnGoHome(lv_event_t *e)      { app_ui_show_screen(UI_SCREEN_HOME); }
void ui_event_btnGoNetwork(lv_event_t *e)   { app_ui_show_screen(UI_SCREEN_NETWORK); }
void ui_event_btnGoStorage(lv_event_t *e)   { app_ui_show_screen(UI_SCREEN_STORAGE); }
void ui_event_btnGoComm(lv_event_t *e)      { app_ui_show_screen(UI_SCREEN_COMM); }
void ui_event_btnGoDiag(lv_event_t *e)      { app_ui_show_screen(UI_SCREEN_DIAGNOSTICS); }
void ui_event_btnGoSettings(lv_event_t *e)  { app_ui_show_screen(UI_SCREEN_SETTINGS); }
void ui_event_btnGoTouch(lv_event_t *e)     { app_ui_show_screen(UI_SCREEN_TOUCH_TEST); }
void ui_event_btnGoAbout(lv_event_t *e)     { app_ui_show_screen(UI_SCREEN_ABOUT); }

void ui_event_btnConnect(lv_event_t *e)
{
    (void)e;
    const char *ssid = lv_textarea_get_text(ui_taSsid);
    const char *pwd = lv_textarea_get_text(ui_taPassword);
    hw_network_connect(ssid, pwd);
    app_ui_update_network_status();
}

void ui_event_btnDisconnect(lv_event_t *e)
{
    (void)e;
    hw_network_disconnect();
    app_ui_update_network_status();
}

void ui_event_ta_focus_show_kb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    app_ui_handle_keyboard_show(ta);
}

void ui_event_keyboard_ready(lv_event_t *e)
{
    (void)e;
    if (ui_taFocused) {
        app_ui_handle_keyboard_hide();
    }
}

void ui_event_keyboard_cancel(lv_event_t *e)
{
    (void)e;
    app_ui_handle_keyboard_hide();
}

void ui_event_slider_backlight(lv_event_t *e)
{
    int32_t v = lv_slider_get_value(ui_sliderBacklight);
    app_ui_apply_backlight_level(v);
}

void ui_event_btn_profile_indoor(lv_event_t *e)
{
    (void)e;
    app_ui_apply_backlight_profile("Interieur", 85);
}

void ui_event_btn_profile_dark(lv_event_t *e)
{
    (void)e;
    app_ui_apply_backlight_profile("Piece sombre", 60);
}

void ui_event_btn_profile_night(lv_event_t *e)
{
    (void)e;
    app_ui_apply_backlight_profile("Nuit", 25);
}

void ui_event_btn_touch_record(lv_event_t *e)
{
    lv_point_t p = {0};
    lv_indev_t *indev = lv_indev_get_act();
    if (indev) {
        lv_indev_get_point(indev, &p);
    }
    char buf[64];
    lv_snprintf(buf, sizeof(buf), "Touch: %d,%d", p.x, p.y);
    lv_label_set_text(ui_lblTouchCoords, buf);
}

void ui_event_dropdown_can(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    uint16_t idx = lv_dropdown_get_selected(dd);
    uint32_t baud = 250000;
    if (idx == 0) baud = 125000;
    else if (idx == 1) baud = 250000;
    else if (idx == 2) baud = 500000;
    else if (idx == 3) baud = 1000000;
    hw_comm_set_can_baudrate(baud);
    app_ui_update_comm_status();
}

void ui_event_dropdown_rs485(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    uint16_t idx = lv_dropdown_get_selected(dd);
    uint32_t baud = 9600;
    if (idx == 0) baud = 9600;
    else if (idx == 1) baud = 19200;
    else if (idx == 2) baud = 38400;
    else if (idx == 3) baud = 57600;
    else if (idx == 4) baud = 115200;
    hw_comm_set_rs485_baudrate(baud);
    app_ui_update_comm_status();
}
