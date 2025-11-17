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

### Dépannage rapide (Windows)

Si la configuration CMake échoue après une mise à jour d’ESP-IDF, vérifiez que l’environnement est chargé
(`export.bat`/`export.ps1`) et relancez une configuration propre :
### Dépannage rapide (Windows / argtable3)

Si `idf.py set-target` ou `idf.py build` échoue avec `file COPY cannot find ... argtable3.h`,
vérifiez votre environnement ESP-IDF puis relancez la configuration :

```powershell
python tools/validate_idf_env.py
idf.py fullclean
idf.py set-target esp32s3
```

Les étapes détaillées sont décrites dans `docs/BUILD_TROUBLESHOOTING.md` (nettoyage complet, vérification de la version ESP-IDF et des dépendances).
Les étapes détaillées sont décrites dans `docs/BUILD_TROUBLESHOOTING.md`.

## Notes d’intégration
- Backlight piloté par LEDC (PWM 20 kHz) sur `BACKLIGHT_GPIO` (configurable dans `app_hw.c`).
- Driver RGB générique (buffers double en PSRAM) via `esp_lcd_new_rgb_panel`; ajuster timings/assignations GPIO selon la carte Waveshare.
- Wi-Fi station avec callbacks UI en cas de connexion/déconnexion (voir `hw_network_connect` / `hw_network_disconnect`).
- microSD montée via `esp_vfs_fat_sdmmc_mount` ; état propagé à l’UI.
- CAN (TWAI) et RS485 configurables ; placeholders pour capture/clear des trames dans l’UI.
- Clavier AZERTY complet (lettres, chiffres, ponctuation, accents) et écran de verrouillage PIN.

Consultez `docs/UI_SPEC.md` pour la correspondance exacte des IDs SquareLine et la palette/styles à appliquer dans SquareLine Studio 1.5.4.
- Documentation fonctionnelle : `docs/UI_SPEC.md`
- Squelette code ESP-IDF : fichiers dans `main/` (app_main, ui_init, app_hw) + `sdkconfig.defaults`, `partitions.csv`.
- Les callbacks et drivers hardware sont fournis sous forme de stubs à compléter.
