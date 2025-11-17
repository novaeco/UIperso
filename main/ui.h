#pragma once
#include "lvgl.h"

// Objets principaux (générés par SquareLine - placeholders)
extern lv_obj_t *ui_ScreenMain;
extern lv_obj_t *ui_kbAzerty;
extern lv_obj_t *ui_taSsid;
extern lv_obj_t *ui_taPwd;

void ui_init(void);
void ui_init_custom(void);

// Callbacks UI
void ui_event_taSsid(lv_event_t *e);
void ui_event_taPwd(lv_event_t *e);
void ui_event_kbReady(lv_event_t *e);
void ui_event_kbCancel(lv_event_t *e);
void ui_event_btnConnect(lv_event_t *e);

// Mise à jour status bar réseau
void app_ui_update_status_bar(void);

