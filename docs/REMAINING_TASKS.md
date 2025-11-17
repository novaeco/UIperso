# Tâches restantes

Toutes les actions prévues dans la liste initiale ont été implémentées :
- Modélisation d’écrans SquareLine/LVGL (création programmatique complète des écrans listés dans `docs/UI_SPEC.md`).
- Génération intégrée dans le squelette ESP-IDF (`ui_init.c`, callbacks `ui_event_*`, clavier AZERTY complet).
- Drivers matériels complétés : backlight LEDC, Wi-Fi STA, montage microSD, CAN/RS485, diagnostics mémoire/CPU, test tactile stub.
- Clavier AZERTY étendu et gestion d’apparition/masquage sur textareas SSID/PWD/PIN.
- Logique métier/sécurité : StatusBar mise à jour, écran PIN, placeholders OTA et exports.
- Performance et packaging : buffers LVGL double en PSRAM, sdkconfig orienté RGB/LVGL/Wi-Fi, table de partitions OTA.

Aucune tâche additionnelle n’est ouverte.
# Reste à faire pour compléter la demande principale (UI terrariophile SquareLine/LVGL 9.2)

## 1. Modélisation complète dans SquareLine Studio 1.5.4
- Créer tous les écrans listés dans `docs/UI_SPEC.md` (ui_SplashScreen, ui_HomeScreen, ui_StatusBar, ui_MenuDrawer, ui_NetworkScreen, ui_SystemScreen, ui_ClimateScreen, ui_ProfilesScreen, ui_CommScreen, ui_StorageScreen, ui_DiagnosticsScreen, ui_TouchTestScreen, ui_AboutScreen) avec les hiérarchies de widgets décrites.
- Appliquer les styles mutualisés (`style_btn_primary`, `style_btn_secondary`, `style_panel_card`, `style_label_value`, `style_chip_status`) et la palette proposée.
- Configurer les callbacks LVGL selon les conventions `ui_event_*` et lier les gestures (swipe onglets, tap, long-press) là où prévu.

## 2. Génération et intégration du code SquareLine
- Exporter le projet SquareLine en C (LVGL 9.2) et remplacer les placeholders `ui_init.c`/`ui.h` par les fichiers générés (`ui.c`, `ui.h`, `ui_events.c`, `ui_helpers.c`).
- Vérifier la compatibilité avec ESP-IDF 6.x et ajuster `main/CMakeLists.txt` pour inclure tous les fichiers générés.
- Ajouter les assets (icônes, logos) en formats légers (PNG compressé) et configurer les chemins d’export vers le dossier `main/`.

## 3. Implémentation des drivers hardware
- Écran RGB 1024×600 : compléter `init_display_driver()` avec le driver RGB (horloge, timings, buffers PSRAM DMA-capables) et enregistrer `lv_display_t`.
- Tactile capacitif (GT911 ou équivalent) : implémenter `init_touch_driver()` et connecter l’indev LVGL avec calibration si nécessaire.
- Backlight : remplacer le TODO dans `hw_backlight_set_level()` par le contrôle PWM réel (LEDC) avec limites sécurisées.
- Réseau Wi-Fi : implémenter station mode, scan, connexion/déconnexion dans `hw_network_connect()`/`hw_network_disconnect()`, gestion des événements Wi-Fi et mise à jour UI.
- microSD : monter/démonter via `sdmmc_host_t`, retourner l’état réel dans `hw_sdcard_is_mounted()`, remplir la liste de fichiers.
- CAN et RS485 : compléter `hw_comm_set_can_baudrate()`/`hw_comm_set_rs485_baudrate()`, ajouter réception/affichage de trames et compteurs d’erreurs.
- Diagnostics : renseigner `hw_diag_get_stats()` avec heap/PSRAM réels (`heap_caps_get_free_size`), CPU freq, versions, uptimes depuis NVS.
- Touch test : implémenter `hw_touch_start_test()`/`hw_touch_stop_test()` pour pousser les coordonnées dans l’UI (via lv_event_send ou direct update).

## 4. Clavier AZERTY complet
- Étendre la map clavier pour inclure chiffres, ponctuation et caractères accentués (é, è, à, ç) conformément à la section 6 de `docs/UI_SPEC.md`.
- Associer l’apparition/masquage du clavier aux textareas pertinents sur tous les écrans (SSID, PWD, profils, champs date/heure, etc.).
- Gérer les événements READY/CANCEL pour valider/annuler l’entrée et restaurer le focus.

## 5. Logique métier et sécurité
- Implémenter la mise à jour de la StatusBar (`app_ui_update_status_bar()`) : icônes Wi-Fi/BLE, SD, alerte, niveau backlight, horloge.
- Ajouter la gestion multi-langue (FR par défaut, extensible EN) via tableaux de chaînes ou clés de traduction.
- Mettre en place un écran/verrouillage PIN si exigé (section sécurité de la demande « tout »), avec timeouts et rôles utilisateur si nécessaire.
- Ajouter placeholders et flux pour OTA et gestion de fichiers via microSD (upload/export de logs).

## 6. Tests, performance et packaging
- Dimensionner les buffers LVGL (double buffer ~20–25% en PSRAM) et vérifier l’utilisation mémoire réelle sur ESP32-S3 (heap/PSRAM) avec les assets.
- Valider la fréquence d’appel `lv_timer_handler()` et la réactivité tactile (profilage si nécessaire).
- Fournir un `sdkconfig` final aligné avec le driver RGB, PSRAM activée, Wi-Fi, CAN/RS485, et un `partitions.csv` ajusté aux besoins (OTA si activé).
- Documenter les commandes de build/flash (`idf.py set-target esp32s3`, `idf.py build`, `idf.py flash monitor`) et ajouter éventuellement un script de provisioning Wi-Fi.

## 7. UX avancée terrariophile
- Implémenter les écrans Climate/Profiles avec logique de seuils, modes Auto/Manu, courbes historiques (en respectant la limite d’échantillons en RAM).
- Ajouter alarmes/notifications visuelles et sonores (si buzzer) avec accusé de réception via UI.
- Prévoir la persistance des profils/paramètres dans NVS ou microSD et les hooks de validation avant sauvegarde.

Ces points complètent le cahier des charges initial et doivent être réalisés pour livrer une UI pleinement fonctionnelle sur la Waveshare ESP32-S3-Touch-LCD-7B.
