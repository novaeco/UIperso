#include "app_ui.h"
#include "ui.h"
#include "app_hw.h"
#include "ui_helpers.h"
#include "esp_log.h"

static const char *TAG = "app_ui";

void app_ui_init_navigation(void)
{
    app_ui_show_screen(UI_SCREEN_SPLASH);
}

void app_ui_show_screen(ui_screen_id_t id)
{
    lv_obj_t *scr = ui_get_screen(id);
    if (scr) {
        ui_set_status_targets(id);
        lv_scr_load(scr);
        app_ui_update_status_bar();
    }
}

void app_ui_update_status_bar(void)
{
    if (!ui_StatusBar) return;
    const char *wifi = hw_network_is_connected() ? "Wi-Fi: connecté" : "Wi-Fi: déconnecté";
    lv_label_set_text(ui_lblStatusWifi, wifi);
    lv_label_set_text(ui_lblStatusTime, "12:00");
    lv_label_set_text(ui_lblStatusAlerts, "OK");
}

void app_ui_update_network_status(void)
{
    char buf[64];
    if (hw_network_is_connected()) {
        lv_snprintf(buf, sizeof(buf), "RSSI: %d dBm", hw_network_get_rssi());
    } else {
        lv_snprintf(buf, sizeof(buf), "Non connecté");
    }
    lv_label_set_text(ui_lblNetworkRssi, buf);
    app_ui_update_status_bar();
}

void app_ui_update_storage_status(void)
{
    hw_sdcard_refresh_status();
    bool mounted = hw_sdcard_is_mounted();
    uint32_t used = hw_sdcard_used_kb();
    uint32_t total = hw_sdcard_total_kb();
    uint32_t pct = (total > 0) ? (used * 100 / total) : 0;
    lv_label_set_text_fmt(ui_lblSdStatus, "microSD: %s", mounted ? "montee" : "absente");
    lv_bar_set_value(ui_barStorage, pct, LV_ANIM_OFF);
    lv_label_set_text_fmt(ui_lblStorage, "%lu / %lu KB (%u%%)", (unsigned long)used, (unsigned long)total, pct);
}

void app_ui_update_comm_status(void)
{
    lv_label_set_text(ui_lblCanStatus, "CAN: OK");
    lv_label_set_text(ui_lblRs485Status, "RS485: OK");
}

void app_ui_update_diag_status(void)
{
    hw_diag_stats_t stats = {0};
    hw_diag_get_stats(&stats);
    lv_label_set_text_fmt(ui_lblDiagStats,
                          "CPU %lu MHz\nHeap %lu KB\nPSRAM %lu KB\nUptime %lus\nReboots %lu",
                          (unsigned long)stats.cpu_freq_mhz,
                          (unsigned long)(stats.heap_free_bytes / 1024),
                          (unsigned long)(stats.psram_free_bytes / 1024),
                          (unsigned long)stats.uptime_s,
                          (unsigned long)stats.reboot_counter);
}

void app_ui_handle_keyboard_show(lv_obj_t *ta)
{
    ui_taFocused = ta;
    ui_keyboard_attach_textarea(ta);
    lv_obj_clear_flag(ui_KbAzerty, LV_OBJ_FLAG_HIDDEN);
}

void app_ui_handle_keyboard_hide(void)
{
    ui_keyboard_detach();
    lv_obj_add_flag(ui_KbAzerty, LV_OBJ_FLAG_HIDDEN);
    ui_taFocused = NULL;
}

void app_ui_apply_backlight_level(int32_t level)
{
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    hw_backlight_set_level((uint8_t)level);
}

void app_ui_apply_backlight_profile(const char *profile_name, uint8_t level)
{
    lv_slider_set_value(ui_sliderBacklight, level, LV_ANIM_OFF);
    hw_backlight_set_profile(profile_name, level);
}
