#include "lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rgb_lcd_port.h"
#include "touch.h"
#include "esp_heap_caps.h"
#include "lv_png.h"
#include "lv_split_jpeg.h"
#include "lv_bmp.h"
#include <stdio.h>
#include <stdlib.h>

#define LVGL_TICK_PERIOD_MS 2
#define LVGL_TASK_PERIOD_MS 5
#define LVGL_BUF_HEIGHT 80

static const char *TAG = "lvgl_port";
static SemaphoreHandle_t lvgl_mutex;
static esp_timer_handle_t tick_timer;

static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true)
    {
        if (lvgl_port_lock(LVGL_TASK_PERIOD_MS))
        {
            lv_timer_handler();
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_PERIOD_MS));
    }
}

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(display);
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(display);
}

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
    uint16_t x[1], y[1];
    uint8_t count = 0;
    esp_lcd_touch_read_data(tp);
    esp_lcd_touch_get_coordinates(tp, x, y, NULL, &count, 1);
    if (count > 0)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x[0];
        data->point.y = y[0];
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void register_input(lv_display_t *display, esp_lcd_touch_handle_t touch_handle)
{
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
    lv_indev_set_user_data(indev, touch_handle);
    (void)display;
}

static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    const char *flags = (mode == LV_FS_MODE_WR) ? "wb" : "rb";
    return fopen(path, flags);
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    fclose((FILE *)file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    (void)drv;
    *br = fread(buf, 1, btr, (FILE *)file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;
    int w;
    switch (whence)
    {
    case LV_FS_SEEK_SET:
        w = SEEK_SET;
        break;
    case LV_FS_SEEK_CUR:
        w = SEEK_CUR;
        break;
    case LV_FS_SEEK_END:
        w = SEEK_END;
        break;
    default:
        w = SEEK_SET;
        break;
    }
    fseek((FILE *)file_p, pos, w);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    *pos_p = ftell((FILE *)file_p);
    return LV_FS_RES_OK;
}

static void lvgl_fs_init(void)
{
    lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    drv.letter = 'S';
    drv.open_cb = fs_open;
    drv.close_cb = fs_close;
    drv.read_cb = fs_read;
    drv.seek_cb = fs_seek;
    drv.tell_cb = fs_tell;
    lv_fs_drv_register(&drv);
}

lv_display_t *lvgl_port_init(esp_lcd_panel_handle_t panel_handle, esp_lcd_touch_handle_t touch_handle)
{
    lv_init();

    lv_color_t *buf1 = heap_caps_aligned_alloc(64, EXAMPLE_LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_aligned_alloc(64, EXAMPLE_LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    lv_display_t *display = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_display_set_user_data(display, panel_handle);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_set_buffers(display, buf1, buf2, EXAMPLE_LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    register_input(display, touch_handle);

    lvgl_mutex = xSemaphoreCreateRecursiveMutex();

    const esp_timer_create_args_t tick_args = {
        .callback = &lv_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick"};
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 4096, NULL, 5, NULL, 0);
    return display;
}

void lvgl_port_register_fs(void)
{
    lvgl_fs_init();
    lv_png_init();
    lv_split_jpeg_init();
    lv_bmp_init();
}

bool lvgl_port_lock(uint32_t timeout_ms)
{
    if (lvgl_mutex == NULL)
    {
        return false;
    }
    return xSemaphoreTakeRecursive(lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void lvgl_port_unlock(void)
{
    if (lvgl_mutex)
    {
        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}
