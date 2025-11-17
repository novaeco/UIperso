# UIperso

Projet ESP-IDF 6.1 + LVGL 9.4 pour carte Waveshare ESP32-S3-Touch-LCD-7B (1024×600). Le code fournit une architecture UI modulaire avec écrans multiples (accueil, réseau, stockage, communication, diagnostics, réglages, test tactile, clavier AZERTY).

## Cible matérielle
- ESP32-S3-WROOM-1-N16R8 (16 Mo flash, 8 Mo PSRAM)
- Affichage RGB 1024×600 (interface parallèle RGB)
- Tactile capacitif GT911 (I2C)
- microSD, RS485, CAN, backlight PWM

## Build rapide
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Arborescence
- `CMakeLists.txt` : projet ESP-IDF
- `sdkconfig.defaults` : options LVGL + PSRAM adaptées à l'ESP32-S3
- `partitions.csv` : partitions flash avec FAT pour microSD virtuelle
- `main/` : sources principales
  - `app_main.c` : init ESP-IDF, LVGL, tâches
  - `ui.c`, `ui.h` : création des écrans LVGL
  - `ui_events.c`, `ui_events.h` : callbacks LVGL
  - `ui_helpers.c`, `ui_helpers.h` : styles, utilitaires UI
  - `app_ui.c`, `app_ui.h` : glue UI/logique métier
  - `app_hw.c`, `app_hw.h` : abstraction hardware (stubs prêts à remplacer)

## Notes
- Le rendu RGB et le driver tactile sont fournis sous forme de placeholders de flush/lecture compatibles LVGL 9.4 ; ils doivent être remplacés par les drivers réels (esp_lcd RGB, GT911 I2C).
- Le clavier virtuel AZERTY avec accents est inclus et s'ouvre automatiquement sur focus des champs texte pertinents.
- Les fonctions hardware exposées dans `app_hw.c` sont volontairement minimalistes (log/valeurs simulées) et peuvent être branchées sur vos drivers.
