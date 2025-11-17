#pragma once
#include "lvgl.h"

void ui_event_btnGoHome(lv_event_t *e);
void ui_event_btnGoNetwork(lv_event_t *e);
void ui_event_btnGoStorage(lv_event_t *e);
void ui_event_btnGoComm(lv_event_t *e);
void ui_event_btnGoDiag(lv_event_t *e);
void ui_event_btnGoSettings(lv_event_t *e);
void ui_event_btnGoTouch(lv_event_t *e);
void ui_event_btnGoAbout(lv_event_t *e);
void ui_event_btnConnect(lv_event_t *e);
void ui_event_btnDisconnect(lv_event_t *e);
void ui_event_ta_focus_show_kb(lv_event_t *e);
void ui_event_keyboard_ready(lv_event_t *e);
void ui_event_keyboard_cancel(lv_event_t *e);
void ui_event_slider_backlight(lv_event_t *e);
void ui_event_btn_profile_indoor(lv_event_t *e);
void ui_event_btn_profile_dark(lv_event_t *e);
void ui_event_btn_profile_night(lv_event_t *e);
void ui_event_btn_touch_record(lv_event_t *e);
void ui_event_dropdown_can(lv_event_t *e);
void ui_event_dropdown_rs485(lv_event_t *e);
