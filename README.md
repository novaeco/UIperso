# UIperso

Spécification et squelette ESP-IDF 6.x + LVGL 9.2 pour une UI terrariophile ciblant la carte Waveshare ESP32-S3-Touch-LCD-7B (1024×600, RGB, tactile capacitif, microSD, RS485, CAN).

- Documentation fonctionnelle : `docs/UI_SPEC.md`
- Squelette code ESP-IDF : fichiers dans `main/` (app_main, ui_init, app_hw) + `sdkconfig.defaults`, `partitions.csv`.
- Les callbacks et drivers hardware sont fournis sous forme de stubs à compléter.
