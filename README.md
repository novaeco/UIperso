# Waveshare ESP32-S3 Touch LCD 7B 1024×600 (ESP32-S3-WROOM-1-N16R8)

## Correctifs récents

* **LVGL tick** : `LV_TICK_CUSTOM` est désormais désactivé dans `components/lvgl/lv_conf.h` pour éviter un double comptage du temps (custom tick + timer `lv_tick_inc()`). Avec le custom tick actif, l’appel du timer d’1 ms déclenchait un assert/panic juste après l’initialisation du GT911. La pile reste inchangée (timer esp_timer + `lv_tick_inc(1)` toutes les 1 ms).
* **Verrou anti-régression du tick** : `app_main` vérifie à la compilation que `LV_TICK_CUSTOM` reste à `0` et loggue la source du tick (`LVGL tick: esp_timer 1 ms`). Cela évite qu’un futur changement de configuration réintroduise le double comptage qui provoquait le reset logiciel après « GT911 ready ».
* **Avertissement checksum** : l’avertissement « Checksum mismatch between flashed and built applications » apparaît lorsque l’ELF utilisé par `idf.py monitor` ne correspond pas à l’image flashée. Utiliser la séquence de build/flash ci-dessous pour garantir la cohérence.

## Séquence de build/flash recommandée (évite le mismatch de checksum)

```powershell
# Dans un ESP-IDF 6.1 (branche master) propre
idf.py fullclean
idf.py set-target esp32s3
idf.py -p COM4 build flash monitor
# En cas de doutes sur l’image déjà présente dans la flash
idf.py erase_flash
idf.py -p COM4 flash monitor
```

## Cause identifiée du reset après « GT911 ready »

* Le panic venait d’un double comptage du tick LVGL : `LV_TICK_CUSTOM=1` combiné au timer `esp_timer` qui appelle `lv_tick_inc(1)` toutes les 1 ms provoquait une assertion LVGL juste après la fin de l’init GT911. La correction force `LV_TICK_CUSTOM=0` et ajoute un _static_assert_ côté application pour empêcher toute régression.

## Journal de démarrage attendu (exemple)

```
I (XXX) MAIN: ESP32-S3 UI phase 4 starting
I (XXX) MAIN: Reset reason: power-on (1)
...
I (XXX) RGB_LCD: RGB panel initialized (1024x600)
I (XXX) GT911: GT911 ready: 1024x600, report 60 Hz
I (XXX) MAIN: LVGL tick: esp_timer 1 ms (LV_TICK_CUSTOM=0)
I (XXX) ui_manager: Interface LVGL prête
# Aucun « rst:0xc (RTC_SW_CPU_RST) » ni « Reset reason: panic (4) » en boucle
```

## Cible matérielle

* Carte Waveshare ESP32-S3 Touch LCD 7B 1024×600
* ESP32-S3-WROOM-1-N16R8 (16 MB Flash, 8 MB PSRAM)
* LCD RGB ST7262 + GT911 (I²C @ 0x5D) + CH422G (I²C @ 0x24)
* I²C partagé : SDA = GPIO 8, SCL = GPIO 9, 100 kHz
* Console UART : GPIO 43/44

