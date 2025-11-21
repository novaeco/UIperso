#include "logs_panel.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lv_theme_custom.h"

#define LOG_PANEL_BUFFER_SIZE 4096
#define LOG_LINE_MAX 256

static lv_obj_t *log_text_area = NULL;
static char log_buffer[LOG_PANEL_BUFFER_SIZE];
static bool log_buffer_initialized = false;

static void ensure_log_buffer_initialized(void)
{
    if (!log_buffer_initialized)
    {
        log_buffer[0] = '\0';
        log_buffer_initialized = true;
    }
}

static void trim_buffer_if_needed(size_t additional)
{
    size_t current = strlen(log_buffer);
    if (current + additional < LOG_PANEL_BUFFER_SIZE - 1)
    {
        return;
    }

    while (current + additional >= LOG_PANEL_BUFFER_SIZE - 1 && current > 0)
    {
        char *newline = strchr(log_buffer, '\n');
        if (!newline)
        {
            log_buffer[0] = '\0';
            current = 0;
            break;
        }
        size_t remove_len = (size_t)(newline - log_buffer) + 1;
        memmove(log_buffer, newline + 1, current - remove_len + 1);
        current = strlen(log_buffer);
    }
}

lv_obj_t *logs_panel_create(void)
{
    ensure_log_buffer_initialized();

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, lv_theme_custom_style_screen(), LV_PART_MAIN);
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(screen, 18, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_add_style(title, lv_theme_custom_style_label(), LV_PART_MAIN);
    lv_label_set_text(title, "Logs");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);

    log_text_area = lv_textarea_create(screen);
    lv_obj_set_size(log_text_area, lv_pct(100), lv_pct(80));
    lv_obj_add_style(log_text_area, lv_theme_custom_style_card(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(log_text_area, lv_color_hex(0x0F131A), LV_PART_MAIN);
    if (log_buffer[0] == '\0')
    {
        snprintf(log_buffer, sizeof(log_buffer), "%s", "Console prête\n");
    }
    else
    {
        trim_buffer_if_needed(strlen("Console prête\n"));
        strncat(log_buffer, "Console prête\n", sizeof(log_buffer) - strlen(log_buffer) - 1);
    }
    log_buffer[sizeof(log_buffer) - 1] = '\0';
    lv_textarea_set_text(log_text_area, log_buffer);
    lv_textarea_set_max_length(log_text_area, LOG_PANEL_BUFFER_SIZE - 1);
    lv_textarea_set_cursor_click_pos(log_text_area, false);
    lv_textarea_set_password_mode(log_text_area, false);
    lv_obj_set_scrollbar_mode(log_text_area, LV_SCROLLBAR_MODE_AUTO);

    return screen;
}

void logs_panel_add_log(const char *fmt, ...)
{
    ensure_log_buffer_initialized();

    if (!fmt)
    {
        return;
    }

    char line[LOG_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    size_t line_len = strnlen(line, sizeof(line));

    trim_buffer_if_needed(line_len + 2);

    strncat(log_buffer, line, LOG_PANEL_BUFFER_SIZE - strlen(log_buffer) - 1);
    strncat(log_buffer, "\n", LOG_PANEL_BUFFER_SIZE - strlen(log_buffer) - 1);

    if (log_text_area)
    {
        lv_textarea_set_text(log_text_area, log_buffer);
        lv_textarea_set_cursor_pos(log_text_area, LV_TEXTAREA_CURSOR_LAST);
    }
}
