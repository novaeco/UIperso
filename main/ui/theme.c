#include "theme.h"

#include <string.h>

static void set_default_text(lv_style_t *style, lv_color_t color, lv_coord_t size)
{
    lv_style_reset(style);
    lv_style_set_text_color(style, color);
    lv_style_set_text_font(style, size >= 20 ? &lv_font_montserrat_20 : &lv_font_montserrat_16);
}

void ui_theme_init(ui_theme_styles_t *styles)
{
    if (styles == NULL)
    {
        return;
    }

    memset(styles, 0, sizeof(*styles));

    lv_style_init(&styles->bg);
    lv_style_set_bg_color(&styles->bg, lv_color_hex(0x0E141C));
    lv_style_set_bg_opa(&styles->bg, LV_OPA_COVER);
    lv_style_set_pad_all(&styles->bg, 16);

    lv_style_init(&styles->card);
    lv_style_set_bg_color(&styles->card, lv_color_hex(0x1B2533));
    lv_style_set_bg_opa(&styles->card, LV_OPA_COVER);
    lv_style_set_radius(&styles->card, 10);
    lv_style_set_pad_all(&styles->card, 12);
    lv_style_set_pad_row(&styles->card, 8);
    lv_style_set_pad_column(&styles->card, 8);
    lv_style_set_border_color(&styles->card, lv_color_hex(0x273142));
    lv_style_set_border_width(&styles->card, 1);
    lv_style_set_shadow_width(&styles->card, 10);
    lv_style_set_shadow_color(&styles->card, lv_color_hex(0x0A0F18));

    lv_style_init(&styles->title);
    set_default_text(&styles->title, lv_color_hex(0xE1E6EE), 20);
    lv_style_set_text_letter_space(&styles->title, 1);

    lv_style_init(&styles->subtle);
    set_default_text(&styles->subtle, lv_color_hex(0x9BA7B8), 16);

    lv_style_init(&styles->badge);
    lv_style_set_bg_color(&styles->badge, lv_color_hex(0x2F3D52));
    lv_style_set_bg_opa(&styles->badge, LV_OPA_COVER);
    lv_style_set_radius(&styles->badge, 8);
    lv_style_set_pad_left(&styles->badge, 8);
    lv_style_set_pad_right(&styles->badge, 8);
    lv_style_set_pad_top(&styles->badge, 4);
    lv_style_set_pad_bottom(&styles->badge, 4);
    set_default_text(&styles->badge, lv_color_hex(0xE6EDF7), 16);

    lv_style_init(&styles->value);
    set_default_text(&styles->value, lv_color_hex(0xE6EDF7), 20);
}

