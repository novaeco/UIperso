#include "ui_helpers.h"

lv_style_t style_panel;
lv_style_t style_title;
lv_style_t style_value;
lv_style_t style_button_primary;
lv_style_t style_button_secondary;
lv_style_t style_status_bar;

void ui_style_init(void)
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, lv_color_hex(0x0F1218));
    lv_style_set_border_color(&style_panel, lv_color_hex(0x2A2F3A));
    lv_style_set_border_width(&style_panel, 2);
    lv_style_set_pad_all(&style_panel, 12);
    lv_style_set_radius(&style_panel, 12);

    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_hex(0xE6EDF7));
    lv_style_set_text_font(&style_title, &lv_font_montserrat_24);

    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, lv_color_hex(0x9CD67E));
    lv_style_set_text_font(&style_value, &lv_font_montserrat_20);

    lv_style_init(&style_button_primary);
    lv_style_set_bg_color(&style_button_primary, lv_color_hex(0x2D8CFF));
    lv_style_set_text_color(&style_button_primary, lv_color_hex(0xFFFFFF));
    lv_style_set_radius(&style_button_primary, 10);
    lv_style_set_pad_all(&style_button_primary, 14);

    lv_style_init(&style_button_secondary);
    lv_style_set_bg_color(&style_button_secondary, lv_color_hex(0x1F2430));
    lv_style_set_text_color(&style_button_secondary, lv_color_hex(0xE6EDF7));
    lv_style_set_radius(&style_button_secondary, 10);
    lv_style_set_pad_all(&style_button_secondary, 12);

    lv_style_init(&style_status_bar);
    lv_style_set_bg_color(&style_status_bar, lv_color_hex(0x111419));
    lv_style_set_pad_all(&style_status_bar, 8);
    lv_style_set_text_color(&style_status_bar, lv_color_hex(0xE6EDF7));
    lv_style_set_text_font(&style_status_bar, &lv_font_montserrat_18);
}
