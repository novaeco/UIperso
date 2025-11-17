# UIperso

Spécification et squelette ESP-IDF 6.x + LVGL 9.2 pour une UI terrariophile ciblant la carte Waveshare ESP32-S3-Touch-LCD-7B (1024×600, RGB, tactile capacitif, microSD, RS485, CAN).

## Contenu
- Documentation fonctionnelle : `docs/UI_SPEC.md` (architecture complète SquareLine/LVGL 9.2, clavier AZERTY, navigation, callbacks).
- Implémentation ESP-IDF : `main/app_main.c` (init LVGL, driver RGB, tick), `main/ui_init.c` (écrans complets, status bar, menu, AZERTY, PIN/OTA), `main/app_hw.c` (backlight LEDC, Wi-Fi STA, microSD, CAN/RS485, diagnostics, test tactile).
- Configuration : `sdkconfig.defaults` (PSRAM, LVGL RGB, Wi-Fi, TWAI), `partitions.csv` (OTA double bank + SPIFFS).

## Construire et flasher
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Notes d’intégration
- Backlight piloté par LEDC (PWM 20 kHz) sur `BACKLIGHT_GPIO` (configurable dans `app_hw.c`).
- Driver RGB générique (buffers double en PSRAM) via `esp_lcd_new_rgb_panel`; ajuster timings/assignations GPIO selon la carte Waveshare.
- Wi-Fi station avec callbacks UI en cas de connexion/déconnexion (voir `hw_network_connect` / `hw_network_disconnect`).
- microSD montée via `esp_vfs_fat_sdmmc_mount` ; état propagé à l’UI.
- CAN (TWAI) et RS485 configurables ; placeholders pour capture/clear des trames dans l’UI.
- Clavier AZERTY complet (lettres, chiffres, ponctuation, accents) et écran de verrouillage PIN.

Consultez `docs/UI_SPEC.md` pour la correspondance exacte des IDs SquareLine et la palette/styles à appliquer dans SquareLine Studio 1.5.4.
