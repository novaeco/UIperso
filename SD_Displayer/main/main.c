
#include "rgb_lcd_port.h"    
#include "gt911.h"           
#include "lvgl_port.h"       
#include "lvgl_demo.h"     
#include "sd.h"

static const char *TAG = "main";  // Tag used for ESP log output

static bool banner_visible = true; // état du bandeau

static lv_obj_t *banner;
static lv_obj_t *dis_img;
static lv_obj_t *play;
static bool is_playing = false;
static char **png_files = NULL;
static int img_number = 0;

static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p);
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p,
                           void *buf, uint32_t btr, uint32_t *br);
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p,
                           uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p);

void create_bottom_banner(lv_obj_t *parent);
static void screen_event_cb(lv_event_t * e);
static void icon1_event_cb(lv_event_t * e);
static void icon2_event_cb(lv_event_t * e);
static void play_event_cb(lv_event_t * e);
void load_new_image(const char *new_path);
void Update_Img();

static int current_img = 0;

void app_main()
{
    static esp_lcd_panel_handle_t panel_handle = NULL;
    static esp_lcd_touch_handle_t tp_handle = NULL; 

    // Initialise GT911 touch screen controller
    tp_handle = touch_gt911_init();  
    
    // Initialise the Waveshare ESP32-S3 RGB LCD hardware
    panel_handle = waveshare_esp32_s3_rgb_lcd_init(); 

    // Allumer LCD backlight
    wavesahre_rgb_lcd_bl_on();   

    // Initialise LVGL library
    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle)); 

    ESP_LOGI(TAG, "Display LVGL demos");

    // Monter la carte SD
    if (sd_mmc_init() == ESP_OK) {
        ESP_LOGI(TAG, "SD Mounted");
        // Lire tous les fichier de la carte SD
        read_files("/sdcard",0);
        // Récupérer tous les fichier
        img_number = list_png_files("/sdcard/images", &png_files);
        // Afficher les fichiers trouvés
        if (img_number > 0) {
            ESP_LOGI(TAG,"PNG file found :\n");
            for (int i = 0; i < img_number; i++) {
                ESP_LOGI(TAG,"  %s\n", png_files[i]);
            }
        } else {
            ESP_LOGI(TAG,"No PNG file found.\n");
        }

        // Initialise le driver FATs
        lv_fs_drv_t drv;
        lv_fs_drv_init(&drv);
        drv.letter   = 'S';
        drv.open_cb  = fs_open;
        drv.close_cb = fs_close;
        drv.read_cb  = fs_read;
        drv.seek_cb  = fs_seek;
        drv.tell_cb  = fs_tell;
        lv_fs_drv_register(&drv);

        // Initialise les decodeurs d'images
        lv_png_init();
        lv_split_jpeg_init();
        lv_bmp_init();

        // Initialise l'affichage principal
        char new_path[512];
        snprintf(new_path, sizeof(new_path), "S:%s", png_files[current_img]);
        dis_img = lv_img_create(lv_scr_act());
        lv_img_set_src(dis_img, new_path);   // ton fichier SD
        lv_obj_center(dis_img);

        // Intialise le panneau lecteur
        create_bottom_banner(lv_scr_act());

    }
    else {
        ESP_LOGI(TAG, "Failed to Mounted");
    }

    while (1)
    {
        // Met à jour les images
        Update_Img();

        vTaskDelay(100); // Delay before the next measurement cycle
    }  
}

void create_bottom_banner(lv_obj_t *parent) {

    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_CLICKED, NULL);

    /* Créer le bandeau */
    banner = lv_obj_create(scr);
    lv_obj_set_size(banner, lv_pct(20), 40);
    lv_obj_align(banner, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    lv_obj_set_style_bg_color(banner, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_pad_all(banner, 5, 0);
    lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *catcher1 = lv_obj_create(banner);
    lv_obj_remove_style_all(catcher1); // objet totalement transparent
    lv_obj_set_size(catcher1, 50, 50);
    lv_obj_add_event_cb(catcher1, icon1_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(catcher1, LV_ALIGN_CENTER, -80, 0); // espacement de 60px

    lv_obj_t *img1 = lv_img_create(catcher1);
    lv_img_set_src(img1, "S:/sdcard/src/arrow_left.png");

    lv_obj_t *catcher3 = lv_obj_create(banner);
    lv_obj_remove_style_all(catcher3); // objet totalement transparent
    lv_obj_set_size(catcher3, 50, 50);
    lv_obj_add_event_cb(catcher3, play_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(catcher3, LV_ALIGN_CENTER, 0, 0); // espacement de 60px

    play = lv_img_create(catcher3);
    lv_img_set_src(play, "S:/sdcard/src/play.png");

    lv_obj_t *catcher2 = lv_obj_create(banner);
    lv_obj_remove_style_all(catcher2); // objet totalement transparent
    lv_obj_set_size(catcher2, 50, 50);
    lv_obj_add_event_cb(catcher2, icon2_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(catcher2, LV_ALIGN_CENTER, 80, 0); // espacement de 60px

    lv_obj_t *img2 = lv_img_create(catcher2);
    lv_img_set_src(img2, "S:/sdcard/src/arrow_right.png");
    
}

void Update_Img(){
    if (!is_playing){
        return;
    }

    vTaskDelay(5000);

    if (!is_playing){
        return;
    }

    ESP_LOGI(TAG,"Changing image\n");

    // Va à l'image suivante
    current_img = (current_img+1)%img_number;
    load_new_image(png_files[current_img]);


}

void load_new_image(const char *img_path) {
    if (!img_path) {
        ESP_LOGI(TAG,"Erreur: path NULL\n");
        return;
    }

    // Charge une image pour un chemin d'accés donné
    if (lvgl_port_lock(0)) {   // prend le mutex LVGL
        // Charger la nouvelle image
        char new_path[512];
        snprintf(new_path, sizeof(new_path), "S:%s", img_path);
        ESP_LOGI(TAG,"Image chargée : %s\n", img_path);
        lv_img_set_src(dis_img, new_path);
        lvgl_port_unlock();
    }

}

// ========================================================================

/* Callback de gestion des clics */
static void screen_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    // Fait disparaitre ou apparaitre le panneau lecteur
    if(code == LV_EVENT_CLICKED) {
        if(banner_visible) {
            lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN); // cacher
            banner_visible = false;
        } else {
            lv_obj_clear_flag(banner, LV_OBJ_FLAG_HIDDEN); // montrer
            banner_visible = true;
        }
        lv_event_stop_bubbling(e); // Evite la propagation
    }
}

/* Callback pour l’icône */
static void icon1_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    // Fait apparaitre l'image d'avant
    if(code == LV_EVENT_CLICKED) {
        if (lvgl_port_lock(0)) {   // prend le mutex LVGL
            ESP_LOGI(TAG,"Left clicked !\n");

            if (current_img == 0) {current_img = img_number - 1;}
            else {current_img--;}
            load_new_image(png_files[current_img]);
            lv_event_stop_bubbling(e); // Evite la propagation
            lvgl_port_unlock();
        }
    }
}

/* Callback pour l’icône */
static void icon2_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    // Fait apparaitre l'image d'après
    if(code == LV_EVENT_CLICKED) {
        if (lvgl_port_lock(0)) {   // prend le mutex LVGL
            ESP_LOGI(TAG,"Right clicked !\n");


            current_img = (current_img+1)%img_number;
            load_new_image(png_files[current_img]);

            lv_event_stop_bubbling(e); // Evite la propagation
            lvgl_port_unlock();
        }
    }
    
}

/* Callback pour play btn */
static void play_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    // Active ou désactive la lecture automatique et change le boutton
    if(code == LV_EVENT_CLICKED) {
        if (lvgl_port_lock(0)) {   // prend le mutex LVGL
            if (is_playing) {
                ESP_LOGI(TAG,"Playing !\n");
                lv_img_set_src(play, "S:/sdcard/src/play.png");
                is_playing = false;
            }
            else {
                ESP_LOGI(TAG,"Pausing !\n");
                lv_img_set_src(play, "S:/sdcard/src/pause.png");
                is_playing = true;
            }

            lv_event_stop_bubbling(e);
            lvgl_port_unlock();
        }
    }
}


// ========================================================================

// Fonction callbacks driver lecteur de fichier FATss

// === Callbacks LVGL pour le FS ===
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    const char *flags = (mode == LV_FS_MODE_WR) ? "wb" : "rb";
    return fopen(path, flags);  // LVGL utilisera ce pointeur
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    fclose((FILE *)file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p,
                           void *buf, uint32_t btr, uint32_t *br)
{
    *br = fread(buf, 1, btr, (FILE *)file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p,
                           uint32_t pos, lv_fs_whence_t whence)
{
    int w;
    switch (whence) {
        case LV_FS_SEEK_SET: w = SEEK_SET; break;
        case LV_FS_SEEK_CUR: w = SEEK_CUR; break;
        case LV_FS_SEEK_END: w = SEEK_END; break;
        default: w = SEEK_SET;
    }
    fseek((FILE *)file_p, pos, w);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    *pos_p = ftell((FILE *)file_p);
    return LV_FS_RES_OK;
}
