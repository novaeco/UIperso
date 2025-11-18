#include "ui_main.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "esp_log.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ui";

#define ICON_WIFI_OFF "S:/sd/icons/wifi_off.png"
#define ICON_WIFI_ON "S:/sd/icons/wifi_on.png"
#define ICON_SD_ON "S:/sd/icons/sdcard.png"
#define ICON_SD_OFF "S:/sd/icons/sdcard_off.png"
#define ICON_BATTERY "S:/sd/icons/battery.png"
#define ICON_PREV "S:/sd/icons/arrow_left.png"
#define ICON_NEXT "S:/sd/icons/arrow_right.png"
#define ICON_REFRESH "S:/sd/icons/refresh.png"

static bool set_image_src_if_exists(lv_obj_t *obj, const char *path)
{
    lv_fs_file_t file;
    if (lv_fs_open(&file, path, LV_FS_MODE_RD) != LV_FS_RES_OK)
    {
        ESP_LOGW(TAG, "Asset missing: %s", path);
        return false;
    }
    lv_fs_close(&file);
    lv_image_set_src(obj, path);
    return true;
}

static void free_image_list(ui_context_t *ctx)
{
    if (!ctx->images)
    {
        return;
    }
    for (int i = 0; i < ctx->image_count; i++)
    {
        free(ctx->images[i]);
    }
    free(ctx->images);
    ctx->images = NULL;
    ctx->image_count = 0;
    ctx->current_index = 0;
}

static void update_image(ui_context_t *ctx, int index)
{
    if (ctx->image_count <= 0 || index < 0 || index >= ctx->image_count)
#define ICON_PLAY "S:/sd/icons/play.png"
#define ICON_PAUSE "S:/sd/icons/pause.png"
#define ICON_REFRESH "S:/sd/icons/refresh.png"

static void update_image(ui_context_t *ctx, int index)
{
    if (ctx->image_count <= 0)
    {
        return;
    }
    if (index < 0 || index >= ctx->image_count)
    {
        return;
    }
    ctx->current_index = index;
    if (lvgl_port_lock(50))
    {
        char path[256];
        snprintf(path, sizeof(path), "S:%s", ctx->images[index]);
        set_image_src_if_exists(ctx->image, path);
        lv_label_set_text_fmt(ctx->title, "%d/%d", index + 1, ctx->image_count);
        const char *name = strrchr(ctx->images[index], '/');
        lv_label_set_text(ctx->filename, name ? name + 1 : ctx->images[index]);
        lv_image_set_src(ctx->image, path);
        lv_label_set_text_fmt(ctx->title, "%d/%d", index + 1, ctx->image_count);
        lvgl_port_unlock();
    }
}

static void next_image(lv_event_t *e)
{
    ui_context_t *ctx = (ui_context_t *)lv_event_get_user_data(e);
    if (ctx->image_count == 0)
    {
        return;
    }
        return;
    int idx = (ctx->current_index + 1) % ctx->image_count;
    update_image(ctx, idx);
}

static void prev_image(lv_event_t *e)
{
    ui_context_t *ctx = (ui_context_t *)lv_event_get_user_data(e);
    if (ctx->image_count == 0)
    {
        return;
    }
        return;
    int idx = (ctx->current_index - 1 + ctx->image_count) % ctx->image_count;
    update_image(ctx, idx);
}

static void rescan_sd(lv_event_t *e)
{
    ui_context_t *ctx = (ui_context_t *)lv_event_get_user_data(e);
    free_image_list(ctx);
    ctx->image_count = list_png_files("/sd/images", &ctx->images);
    ctx->sd_present = ctx->image_count >= 0;
    if (ctx->image_count < 0)
    {
        ctx->image_count = 0;
        ctx->sd_present = false;
    }
    if (ctx->images)
    {
        for (int i = 0; i < ctx->image_count; i++)
        {
            free(ctx->images[i]);
        }
        free(ctx->images);
        ctx->images = NULL;
    }
    ctx->image_count = list_png_files("/sdcard/images", &ctx->images);
    if (ctx->image_count > 0)
    {
        update_image(ctx, 0);
    }
    else if (lvgl_port_lock(50))
    {
        lv_label_set_text(ctx->title, "Aucune image");
        lv_label_set_text(ctx->filename, "");
        lvgl_port_unlock();
    }
}

static void build_status_bar(lv_obj_t *parent, ui_context_t *ctx)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 64);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x151515), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bar, 10, 0);
    lv_obj_set_style_radius(bar, 8, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x2e7d32), 0);
    lv_obj_set_style_border_width(bar, 1, 0);

    ctx->wifi_icon = lv_image_create(bar);
    set_image_src_if_exists(ctx->wifi_icon, ICON_WIFI_OFF);

    ctx->sd_icon = lv_image_create(bar);
    set_image_src_if_exists(ctx->sd_icon, ICON_SD_OFF);

    ctx->battery_icon = lv_image_create(bar);
    set_image_src_if_exists(ctx->battery_icon, ICON_BATTERY);
}

static void build_status_bar(ui_context_t *ctx)
{
    lv_obj_t *bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(bar, lv_pct(100), 60);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1b1b1b), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bar, 8, 0);
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);

    ctx->wifi_icon = lv_image_create(bar);
    lv_image_set_src(ctx->wifi_icon, ICON_WIFI_OFF);

    ctx->sd_icon = lv_image_create(bar);
    lv_image_set_src(ctx->sd_icon, ICON_SD_OFF);

    ctx->battery_icon = lv_image_create(bar);
    lv_image_set_src(ctx->battery_icon, ICON_BATTERY);

    ctx->clock_label = lv_label_create(bar);
    lv_obj_set_style_text_color(ctx->clock_label, lv_color_hex(0xc9a646), 0);
    lv_label_set_text(ctx->clock_label, "--:--:--");
    lv_obj_set_flex_grow(ctx->clock_label, 1);
    lv_obj_set_style_text_align(ctx->clock_label, LV_TEXT_ALIGN_RIGHT, 0);
}

static void build_controls(lv_obj_t *parent, ui_context_t *ctx)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_height(panel, 110);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1c1c1c), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(panel, 14, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2e7d32), 0);
    lv_obj_set_style_border_width(panel, 1, 0);

    lv_obj_t *btn_prev = lv_button_create(panel);
    lv_obj_set_size(btn_prev, 90, 70);
    lv_obj_add_event_cb(btn_prev, prev_image, LV_EVENT_CLICKED, ctx);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x263428), 0);
    lv_obj_set_style_radius(btn_prev, 10, 0);
    lv_obj_t *img_prev = lv_image_create(btn_prev);
    set_image_src_if_exists(img_prev, ICON_PREV);
    lv_obj_center(img_prev);

    lv_obj_t *btn_next = lv_button_create(panel);
    lv_obj_set_size(btn_next, 90, 70);
    lv_obj_add_event_cb(btn_next, next_image, LV_EVENT_CLICKED, ctx);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x263428), 0);
    lv_obj_set_style_radius(btn_next, 10, 0);
    lv_obj_t *img_next = lv_image_create(btn_next);
    set_image_src_if_exists(img_next, ICON_NEXT);
    lv_obj_center(img_next);

    lv_obj_t *btn_rescan = lv_button_create(panel);
    lv_obj_set_size(btn_rescan, 90, 70);
    lv_obj_add_event_cb(btn_rescan, rescan_sd, LV_EVENT_CLICKED, ctx);
    lv_obj_set_style_bg_color(btn_rescan, lv_color_hex(0x2b2b2b), 0);
    lv_obj_set_style_radius(btn_rescan, 10, 0);
    lv_obj_t *img_ref = lv_image_create(btn_rescan);
    set_image_src_if_exists(img_ref, ICON_REFRESH);
static void build_controls(ui_context_t *ctx)
{
    lv_obj_t *panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel, lv_pct(100), 100);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1f1f1f), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *btn_prev = lv_button_create(panel);
    lv_obj_set_size(btn_prev, 80, 64);
    lv_obj_add_event_cb(btn_prev, prev_image, LV_EVENT_CLICKED, ctx);
    lv_obj_t *img_prev = lv_image_create(btn_prev);
    lv_image_set_src(img_prev, ICON_PREV);
    lv_obj_center(img_prev);

    lv_obj_t *btn_next = lv_button_create(panel);
    lv_obj_set_size(btn_next, 80, 64);
    lv_obj_add_event_cb(btn_next, next_image, LV_EVENT_CLICKED, ctx);
    lv_obj_t *img_next = lv_image_create(btn_next);
    lv_image_set_src(img_next, ICON_NEXT);
    lv_obj_center(img_next);

    lv_obj_t *btn_rescan = lv_button_create(panel);
    lv_obj_set_size(btn_rescan, 80, 64);
    lv_obj_add_event_cb(btn_rescan, rescan_sd, LV_EVENT_CLICKED, ctx);
    lv_obj_t *img_ref = lv_image_create(btn_rescan);
    lv_image_set_src(img_ref, ICON_REFRESH);
    lv_obj_center(img_ref);
}

void ui_init(lv_display_t *display, ui_context_t *ctx)
{
    (void)display;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0f0f0f), 0);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x090909), 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_pad_all(scr, 12, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(scr, 12, 0);

    ctx->last_wifi_state = -1;
    ctx->last_sd_present = false;

    build_status_bar(scr, ctx);

    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x141414), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(content, 12, 0);
    lv_obj_set_style_pad_all(content, 16, 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x2e7d32), 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 10, 0);

    ctx->title = lv_label_create(content);
    lv_label_set_text(ctx->title, "Images");
    lv_obj_set_style_text_color(ctx->title, lv_color_hex(0x2e7d32), 0);
    lv_obj_set_style_text_font(ctx->title, &lv_font_montserrat_24, 0);

    ctx->image = lv_image_create(content);
    lv_obj_set_size(ctx->image, 940, 450);
    lv_image_set_scale_mode(ctx->image, LV_IMAGE_SCALE_MODE_FIT);
    lv_obj_set_style_radius(ctx->image, 12, 0);
    lv_obj_set_style_bg_color(ctx->image, lv_color_hex(0x0c0c0c), 0);
    lv_obj_set_style_bg_opa(ctx->image, LV_OPA_80, 0);

    ctx->filename = lv_label_create(content);
    lv_label_set_text(ctx->filename, "");
    lv_obj_set_style_text_color(ctx->filename, lv_color_hex(0xc9a646), 0);
    lv_obj_set_style_text_font(ctx->filename, &lv_font_montserrat_16, 0);

    build_controls(scr, ctx);

    ctx->image_count = list_png_files("/sd/images", &ctx->images);
    ctx->sd_present = ctx->image_count >= 0;
    if (ctx->image_count < 0)
    {
        ctx->image_count = 0;
        ctx->sd_present = false;
    }
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x111111), 0);

    ctx->image = lv_image_create(scr);
    lv_obj_center(ctx->image);
    lv_obj_set_size(ctx->image, 900, 520);
    lv_obj_set_style_radius(ctx->image, 8, 0);

    ctx->title = lv_label_create(scr);
    lv_obj_align(ctx->title, LV_ALIGN_TOP_MID, 0, 70);
    lv_label_set_text(ctx->title, "Images");
    lv_obj_set_style_text_color(ctx->title, lv_color_hex(0x2e7d32), 0);

    build_status_bar(ctx);
    build_controls(ctx);

    ctx->image_count = list_png_files("/sdcard/images", &ctx->images);
    if (ctx->image_count > 0)
    {
        update_image(ctx, 0);
    }
    else
    {
        lv_label_set_text(ctx->title, "Aucune image");
    }
}

void ui_update_status(ui_context_t *ctx)
{
    if (!lvgl_port_lock(10))
    {
        return;
    }
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &t);
    lv_label_set_text(ctx->clock_label, buf);

    wifi_config_runtime_t wifi = wifi_manager_get_state();
    if (wifi.state != ctx->last_wifi_state)
    {
        if (wifi.state == WIFI_STATE_CONNECTED)
        {
            set_image_src_if_exists(ctx->wifi_icon, ICON_WIFI_ON);
        }
        else if (wifi.state == WIFI_STATE_CONNECTING)
        {
            set_image_src_if_exists(ctx->wifi_icon, ICON_REFRESH);
        }
        else
        {
            set_image_src_if_exists(ctx->wifi_icon, ICON_WIFI_OFF);
        }
        ctx->last_wifi_state = wifi.state;
    }

    if (ctx->sd_present != ctx->last_sd_present)
    {
        if (ctx->sd_present)
        {
            set_image_src_if_exists(ctx->sd_icon, ICON_SD_ON);
        }
        else
        {
            set_image_src_if_exists(ctx->sd_icon, ICON_SD_OFF);
        }
        ctx->last_sd_present = ctx->sd_present;
    }
    lvgl_port_unlock();
    if (wifi.state == WIFI_STATE_CONNECTED)
    {
        lv_image_set_src(ctx->wifi_icon, ICON_WIFI_ON);
    }
    else if (wifi.state == WIFI_STATE_CONNECTING)
    {
        lv_image_set_src(ctx->wifi_icon, ICON_REFRESH);
    }
    else
    {
        lv_image_set_src(ctx->wifi_icon, ICON_WIFI_OFF);
    }

    if (ctx->image_count > 0)
    {
        lv_image_set_src(ctx->sd_icon, ICON_SD_ON);
    }
    else
    {
        lv_image_set_src(ctx->sd_icon, ICON_SD_OFF);
    }
}
