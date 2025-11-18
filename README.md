# ESP32-S3 Touch LCD 7B - ESP-IDF 6.1 + LVGL 9.4 Template

Projet complet prêt à compiler pour la carte Waveshare ESP32-S3-Touch-LCD-7B (1024x600, ESP32-S3-WROOM-1-N16R8) intégrant l'écran RGB, le tactile GT911, la SD SPI (CS via CH422G EXIO4), le CAN, le RS485, l'expandeur CH422G et une interface LVGL 9.4 multi-écran.

## Build rapide (ESP-IDF v6.1)
```
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash
idf.py -p COMx monitor
```

Configurez le port série selon votre environnement. Les options clés sont pré-définies dans `sdkconfig.defaults` (PSRAM activée, LVGL, RGB LCD, CAN, FATFS).
