#include "system_panel.h"

#include "esp_err.h"
#include "esp_log.h"

#include "ch422g.h"
#include "cs8501.h"
#include "logs_panel.h"
#include "lv_theme_custom.h"
#include "sdcard.h"

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
}

static void refresh_system_info(lv_timer_t *timer)
{
    (void)timer;
    if (sd_status_label)
    {
        lv_label_set_text_fmt(sd_status_label, "SD : %s", sdcard_is_mounted() ? "montée" : "non montée");
    }

    if (battery_label)
    {
        float voltage = cs8501_get_battery_voltage();
        if (cs8501_has_voltage_reading() && !isnan(voltage))
        {
            lv_label_set_text_fmt(battery_label, "Batterie : %.2f V", voltage);
        }
        else
        {
            lv_label_set_text(battery_label, "Batterie : N/A");
        }
    }

    if (charge_label)
    {
        if (cs8501_has_charge_status())
        {
            lv_label_set_text_fmt(charge_label, "Charge : %s", cs8501_is_charging() ? "en cours" : "inactive");
        }
        else
        {
            lv_label_set_text(charge_label, "Charge : inconnue");
        }
    }
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

    refresh_system_info(NULL);

    if (!system_timer)
    {
        system_timer = lv_timer_create(refresh_system_info, SYSTEM_REFRESH_PERIOD_MS, NULL);
    }

    apply_backlight_level(80);

    return screen;
}
