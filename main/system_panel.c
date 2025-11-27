#include "system_panel.h"

#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "project_config_compat.h"

#include "ch422g.h"
#include "cs8501.h"
#include "logs_panel.h"
#include "lv_theme_custom.h"
#include "system_status.h"
#if CONFIG_ENABLE_SDCARD
#include "sdcard.h"
#endif

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>

#define SYSTEM_REFRESH_PERIOD_MS 1500

static const char *TAG = "system_panel";

static lv_obj_t *sd_status_label = NULL;
static lv_obj_t *brightness_value_label = NULL;
static lv_obj_t *brightness_switch = NULL;
static lv_obj_t *battery_label = NULL;
static lv_obj_t *charge_label = NULL;
static lv_timer_t *system_timer = NULL;
static lv_obj_t *i2c_status_label = NULL;
static lv_obj_t *ch422g_status_label = NULL;
static lv_obj_t *gt911_status_label = NULL;
static lv_obj_t *touch_status_label = NULL;
static lv_obj_t *can_status_label = NULL;
static lv_obj_t *rs485_status_label = NULL;

static void set_status_label(lv_obj_t *label, const char *prefix, bool ok)
{
    if (!label)
    {
        return;
    }

    lv_label_set_text_fmt(label, "%s : %s", prefix, ok ? "OK" : "KO");
    lv_obj_set_style_text_color(label, ok ? lv_color_hex(0x4CAF50) : lv_color_hex(0xF44336), LV_PART_MAIN);
}

static void set_status_text_with_color(lv_obj_t *label, const char *text, bool ok, bool known)
{
    if (!label || !text)
    {
        return;
    }

    lv_label_set_text(label, text);
    lv_color_t color = lv_color_hex(0x9E9E9E);
    if (known)
    {
        color = ok ? lv_color_hex(0x4CAF50) : lv_color_hex(0xF44336);
    }
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
}

static void apply_backlight_level(uint8_t percent)
{
    percent = percent > 100 ? 100 : percent;
    if (brightness_value_label)
    {
        lv_label_set_text_fmt(brightness_value_label, "%s", percent < 5 ? "Éteint" : "Allumé");
    }

    if (percent < 5)
    {
        ch422g_set_backlight(false);
    }
    else
    {
        ch422g_set_backlight(true);
    }

    ESP_LOGI(TAG, "Rétroéclairage binaire : %s", percent < 5 ? "OFF" : "ON");
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
    {
        return;
    }
    if (!brightness_switch)
    {
        return;
    }

    bool on = lv_obj_has_state(brightness_switch, LV_STATE_CHECKED);
    apply_backlight_level(on ? 100 : 0);
}

static void sd_test_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

#if !CONFIG_ENABLE_SDCARD
    logs_panel_add_log("Test SD : désactivée");
    return;
#else
    if (!sdcard_is_mounted())
    {
        logs_panel_add_log("Test SD : carte absente");
        return;
    }

    esp_err_t err = sdcard_test_file();
    if (err == ESP_OK)
    {
        logs_panel_add_log("Test SD : succès");
    }
    else
    {
        logs_panel_add_log("Test SD : erreur (%s)", esp_err_to_name(err));
    }
#endif
}

static void refresh_system_info(lv_timer_t *timer)
{
    (void)timer;
    system_status_t status = {0};

#if CONFIG_ENABLE_POWER
    float voltage = cs8501_get_battery_voltage();
    const bool voltage_available = cs8501_has_voltage_reading() && !isnan(voltage);
    const bool charging_known = cs8501_has_charge_status();
    const bool charging = charging_known ? cs8501_is_charging() : false;
    system_status_set_power(true, voltage_available, voltage_available ? voltage : NAN, charging_known, charging);
#else
    system_status_set_power(false, false, NAN, false, false);
#endif

    system_status_get(&status);

    if (sd_status_label)
    {
#if CONFIG_ENABLE_SDCARD
        sdcard_status_t sd_status = sdcard_get_status();
        const char *state = sd_status.mounted ? "montée" : "non montée";
        lv_label_set_text_fmt(sd_status_label, "SD : %s", state);
#else
        lv_label_set_text(sd_status_label, "SD : désactivée");
#endif
    }

    if (battery_label)
    {
        if (status.power_ok && status.power_telemetry_available && !isnan(status.vbat))
        {
            lv_label_set_text_fmt(battery_label, "Batterie : %.2f V", status.vbat);
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0x4CAF50), LV_PART_MAIN);
        }
        else if (!status.power_ok)
        {
            lv_label_set_text(battery_label, "Batterie : désactivée");
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0x9E9E9E), LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text(battery_label, "Batterie : N/A");
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0x9E9E9E), LV_PART_MAIN);
        }
    }

    if (charge_label)
    {
        if (status.power_ok && status.charging_known)
        {
            lv_label_set_text_fmt(charge_label, "Charge : %s", status.charging ? "en cours" : "inactive");
            lv_obj_set_style_text_color(charge_label, status.charging ? lv_color_hex(0x4CAF50) : lv_color_hex(0xFFB300), LV_PART_MAIN);
        }
        else if (!status.power_ok)
        {
            lv_label_set_text(charge_label, "Charge : désactivée");
            lv_obj_set_style_text_color(charge_label, lv_color_hex(0x9E9E9E), LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text(charge_label, "Charge : inconnue");
            lv_obj_set_style_text_color(charge_label, lv_color_hex(0x9E9E9E), LV_PART_MAIN);
        }
    }

#if CONFIG_ENABLE_CAN
    if (can_status_label)
    {
        lv_label_set_text_fmt(can_status_label, "CAN : %s (%" PRIu32 " rx)", status.can_ok ? "OK" : "KO", status.can_frames_rx);
        lv_obj_set_style_text_color(can_status_label, status.can_ok ? lv_color_hex(0x4CAF50) : lv_color_hex(0xF44336), LV_PART_MAIN);
    }
#else
    set_status_text_with_color(can_status_label, "CAN : désactivé", false, false);
#endif

#if CONFIG_ENABLE_RS485
    if (rs485_status_label)
    {
        lv_label_set_text_fmt(rs485_status_label, "RS485 : %s (tx=%" PRIu32 "B rx=%" PRIu32 "B)",
                              status.rs485_ok ? "OK" : "KO",
                              status.rs485_tx_count,
                              status.rs485_rx_count);
        lv_obj_set_style_text_color(rs485_status_label, status.rs485_ok ? lv_color_hex(0x4CAF50) : lv_color_hex(0xF44336), LV_PART_MAIN);
    }
#else
    set_status_text_with_color(rs485_status_label, "RS485 : désactivé", false, false);
#endif
}

static lv_obj_t *create_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, lv_theme_custom_style_card(), LV_PART_MAIN);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(card);
    lv_obj_add_style(label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);

    return card;
}

lv_obj_t *system_panel_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, lv_theme_custom_style_screen(), LV_PART_MAIN);
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(screen, 18, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_add_style(title, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(title, "Système");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);

    lv_obj_t *screen_section = create_section(screen, "Écran");
    brightness_switch = lv_switch_create(screen_section);
    lv_obj_add_event_cb(brightness_switch, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_state(brightness_switch, LV_STATE_CHECKED);
    brightness_value_label = lv_label_create(screen_section);
    lv_obj_add_style(brightness_value_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(brightness_value_label, "Allumé");

    lv_obj_t *storage_section = create_section(screen, "Stockage");
    sd_status_label = lv_label_create(storage_section);
    lv_obj_add_style(sd_status_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(sd_status_label, "SD : inconnue");
    lv_obj_t *sd_btn = lv_btn_create(storage_section);
    lv_obj_add_style(sd_btn, lv_theme_custom_style_button(), LV_PART_MAIN);
    lv_obj_add_event_cb(sd_btn, sd_test_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *sd_btn_label = lv_label_create(sd_btn);
    lv_label_set_text(sd_btn_label, "Test SD");
    lv_obj_center(sd_btn_label);

    lv_obj_t *power_section = create_section(screen, "Alimentation");
    battery_label = lv_label_create(power_section);
    lv_obj_add_style(battery_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(battery_label, "Batterie : --");
    charge_label = lv_label_create(power_section);
    lv_obj_add_style(charge_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(charge_label, "Charge : --");

    lv_obj_t *io_section = create_section(screen, "Bus & tactile");
    i2c_status_label = lv_label_create(io_section);
    lv_obj_add_style(i2c_status_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(i2c_status_label, "I2C : inconnu");

    ch422g_status_label = lv_label_create(io_section);
    lv_obj_add_style(ch422g_status_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(ch422g_status_label, "CH422G : inconnu");

    gt911_status_label = lv_label_create(io_section);
    lv_obj_add_style(gt911_status_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(gt911_status_label, "GT911 : inconnu");

    touch_status_label = lv_label_create(io_section);
    lv_obj_add_style(touch_status_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(touch_status_label, "Tactile : inconnu");

    can_status_label = lv_label_create(io_section);
    lv_obj_add_style(can_status_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(can_status_label, "CAN : inconnu");

    rs485_status_label = lv_label_create(io_section);
    lv_obj_add_style(rs485_status_label, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(rs485_status_label, "RS485 : inconnu");

    refresh_system_info(NULL);

    if (!system_timer)
    {
        system_timer = lv_timer_create(refresh_system_info, SYSTEM_REFRESH_PERIOD_MS, NULL);
    }

    apply_backlight_level(80);

    set_status_label(i2c_status_label, "I2C", false);
    set_status_label(ch422g_status_label, "CH422G", false);
    set_status_label(gt911_status_label, "GT911", false);
    set_status_label(touch_status_label, "Tactile", false);
    set_status_text_with_color(can_status_label, "CAN : désactivé", false, false);
    set_status_text_with_color(rs485_status_label, "RS485 : désactivé", false, false);

    return screen;
}

void system_panel_set_bus_status(bool i2c_ok, bool ch422g_ok, bool gt911_ok)
{
    set_status_label(i2c_status_label, "I2C", i2c_ok);
    set_status_label(ch422g_status_label, "CH422G", ch422g_ok);
    set_status_label(gt911_status_label, "GT911", gt911_ok);
}

void system_panel_set_touch_status(bool touch_ok)
{
    set_status_label(touch_status_label, "Tactile", touch_ok);
}
