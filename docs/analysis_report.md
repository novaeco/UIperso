# Analyse du projet ESP-IDF / LVGL (ESP32-S3 Touch LCD 7B)

## 1) Résumé exécutif
- Projet ciblant ESP32-S3 avec LVGL 9.4, écran RGB 1024×600 et tactile GT911 via bus I²C partagé.
- Configuration sdkconfig minimale : PSRAM 8 Mo activée, tick FreeRTOS à 1 kHz, polices LVGL activées.
- LVGL configuré en RGB565 avec draw buffers en PSRAM (2 × ~200 KiB) mais allocation interne LVGL limitée à 128 KiB.
- Pipeline d’affichage `esp_lcd` initialisé mais la mise sous tension du panneau (`esp_lcd_panel_disp_on_off`) n’est jamais appelée.
- Table de partitions/flash 16 Mo non déclarée : risque d’image flashée sur 4 Mo par défaut.

## 2) Problèmes détectés et correctifs proposés

### P1 – Taille flash/partitions non alignée avec le module 16 Mo
- **Contexte** : `sdkconfig` n’explicite pas `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` ni une table de partitions 16 Mo ; l’IDF retombe sur la table par défaut 4 Mo, limitant OTA et pouvant tronquer la flash.
- **Correction** : définir la taille flash et une table partitions custom 16 Mo.
  ```ini
  # sdkconfig.defaults
  CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
  CONFIG_PARTITION_TABLE_CUSTOM=y
  CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
  CONFIG_PARTITION_TABLE_MD5=y
  ```
  Exemple de `partitions.csv` (à placer à la racine) :
  ```csv
  # Name,   Type, SubType, Offset,  Size
  nvs,      data, nvs,     0x9000,  0x6000
  phy_init, data, phy,     0xf000,  0x1000
  factory,  app,  factory, 0x10000, 0x500000
  ota_0,    app,  ota_0,   0x510000,0x500000
  ota_1,    app,  ota_1,   0xa10000,0x500000
  storage,  data, fat,     0xf10000,0x0EF0000
  ```

### P2 – Panneau RGB non activé après init
- **Contexte** : `rgb_lcd_init()` crée le panel et appelle `esp_lcd_panel_init()` mais ne l’active jamais ; certains contrôleurs restent noirs tant que `esp_lcd_panel_disp_on_off(panel, true)` n’a pas été émis.
- **Correction (diff cible `components/display/rgb_lcd.c`)** :
  ```c
  err = esp_lcd_panel_init(s_panel_handle);
  if (err != ESP_OK) { /* log */ return; }
+
+    err = esp_lcd_panel_disp_on_off(s_panel_handle, true);
+    if (err != ESP_OK) {
+        ESP_LOGE(TAG, "RGB panel on/off failed: %s", esp_err_to_name(err));
+        return;
+    }
  ```

### P3 – Heap LVGL interne trop faible pour 1024×600
- **Contexte** : `lv_conf.h` fixe `LV_MEM_SIZE` à 128 KiB et `LV_MEM_ADR=0`, donc la heap LVGL reste en RAM interne alors que les draw buffers consomment ~400 KiB en PSRAM. Les écrans multiples + styles 24 px satureront vite la heap.
- **Correction** : basculer la heap LVGL vers la PSRAM et augmenter la taille.
  ```c
  // components/lvgl/lv_conf.h
-#define LV_MEM_SIZE               (128U * 1024U)
-#define LV_MEM_ADR                0
+#define LV_MEM_CUSTOM             1
+#define LV_MEM_CUSTOM_INCLUDE     "esp_heap_caps.h"
+#define LV_MEM_CUSTOM_ALLOC       heap_caps_malloc
+#define LV_MEM_CUSTOM_FREE        heap_caps_free
+#define LV_MEM_CUSTOM_REALLOC     heap_caps_realloc
+#define LV_MEM_CUSTOM_GET_SIZE    heap_caps_get_allocated_size
+#define LV_MEM_CUSTOM_ATTR        MALLOC_CAP_SPIRAM
+#define LV_MEM_SIZE               (256U * 1024U)
+// LV_MEM_ADR unused when LV_MEM_CUSTOM=1
  ```

### P4 – Journal TWAI incohérent sur le mode de fonctionnement
- **Contexte** : `can_bus_init()` installe `TWAI_GENERAL_CONFIG_DEFAULT(..., TWAI_MODE_NORMAL)` mais logge « loopback mode ». Le diagnostic terrain est trompeur et peut masquer l’absence de transceiver/filtrage.
- **Correction** : aligner le log (ou activer le vrai loopback en développement).
  ```c
-    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(..., TWAI_MODE_NORMAL);
+    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(..., TWAI_MODE_NORMAL);
     ...
-    ESP_LOGI(TAG, "TWAI bus initialized (loopback mode, 500 kbit/s)");
+    ESP_LOGI(TAG, "TWAI bus initialized (normal mode, 500 kbit/s)");
  ```

### P5 – Séquence d’allocation LCD non protégée contre l’absence de PSRAM
- **Contexte** : les deux draw buffers LVGL sont alloués en PSRAM (MALLOC_CAP_SPIRAM) sans vérifier que l’option PSRAM est active côté runtime (`esp_psram_is_initialized`). En cas de boot sans PSRAM, l’init LCD retourne simplement, laissant LVGL sans écran mais sans log explicite.
- **Correction** : vérifier PSRAM et logger une erreur bloquante pour diagnostiquer.
  ```c
  // components/display/rgb_lcd.c, avant les malloc
+    if (!esp_psram_is_initialized()) {
+        ESP_LOGE(TAG, "PSRAM not initialized: cannot allocate LVGL draw buffers");
+        return;
+    }
  ```

## 3) Points d’attention supplémentaires
- `LV_TICK_CUSTOM` est bien à 0, cohérent avec le timer esp_timer de 1 ms dans `main.c`.
- Le bus I²C partagé utilise `glitch_ignore_cnt=7`; conserver 100 kHz pour GT911/CH422G (ok avec câblage Waveshare SDA=8, SCL=9).
- Les GPIO CAN (43/44) sont en TODO : confirmer avec le schéma Waveshare avant câblage transceiver.

## 4) Plan d’action priorisé
1. Appliquer P2 pour obtenir un affichage actif (corrige écran noir après init).
2. Appliquer P1 pour sécuriser la flash/partitions 16 Mo avant tout flash terrain.
3. Appliquer P3 pour éviter les OOM LVGL sur UI complexes.
4. Appliquer P5 pour diagnostiquer les boots sans PSRAM.
5. Corriger le log TWAI (P4) pour fiabiliser les tests terrain.

## 5) Commandes conseillées
- `idf.py fullclean && idf.py set-target esp32s3 && idf.py build`
- `idf.py -p COM4 flash monitor` pour valider l’absence de reset/panic après init LCD/GT911.
