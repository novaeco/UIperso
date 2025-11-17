# Spécification UI terrariophile – LVGL 9.2 / SquareLine 1.5.4 – ESP32-S3-Touch-LCD-7B

## 2) Architecture UI & navigation
- **Application** : Contrôleur terrariophile avancé (gestion environnementale multi-capteurs, actionneurs, alarmes, profils).
- **Modèle de navigation** : barre supérieure de statut fixe + panneau latéral (menu burger) + zone de contenu centrale à onglets pour les modules fréquents. Gestes autorisés : swipe horizontal pour changer d’onglet dans les sections Dashboard/Logs/Diagnostics ; tap, long-press sur cartes et sliders ; scroll inertiel sur listes.
- **Écrans principaux SquareLine (IDs exacts)** :
  - `ui_SplashScreen`
  - `ui_HomeScreen`
  - `ui_StatusBar` (conteneur réutilisable en haut des écrans)
  - `ui_MenuDrawer` (panneau latéral caché/repliable)
  - `ui_NetworkScreen`
  - `ui_SystemScreen`
  - `ui_ClimateScreen` (capteurs/actuateurs terrarium)
  - `ui_ProfilesScreen` (profils/scénarios/programmations)
  - `ui_CommScreen` (CAN/RS485)
  - `ui_StorageScreen` (microSD)
  - `ui_DiagnosticsScreen`
  - `ui_TouchTestScreen`
  - `ui_AboutScreen`

### Tableau synthétique
| Écran (ID) | Rôle | Widgets clés | Interactions majeures | Fréquence |
|---|---|---|---|---|
| ui_SplashScreen | Logo, init système, progression | Image/logo, barre de progression, label état | Timer auto->Home, éventuel tap pour passer | Rare (boot) |
| ui_HomeScreen | Dashboard synthétique | Cartes valeurs (temp/hygro/CO₂), boutons accès rapide, onglets mini-graphes | Tap carte -> détails climate, swipe onglets, refresh périodique | Très fréquent |
| ui_MenuDrawer | Navigation latérale | Liste boutons (Home, Réseau, Système, Climate, Profils, Comm, Storage, Diag, About), avatar/nom profil | LV_EVENT_CLICKED sur boutons, swipe depuis bord gauche | Fréquent |
| ui_StatusBar | Bandeau état global | Icônes Wi-Fi/BLE, horloge, alerte, SD, luminosité | Clic icône -> popups rapides (Wi-Fi, brightness), maj via app_ui_update_status_bar() | Très fréquent |
| ui_NetworkScreen | Wi-Fi/BLE | Liste SSID, textarea SSID/pwd, boutons scan/connexion/déconnexion, labels IP/RSSI, toggle BLE | Click, VALUE_CHANGED sur list/textarea, affichage clavier, ready/cancel sur connexion | Fréquent |
| ui_SystemScreen | Système & backlight | Sliders luminosité/backlight, toggles jour/nuit, réglage date/heure, langue | VALUE_CHANGED sliders, boutons presets, ouverture clavier pour texte | Fréquent |
| ui_ClimateScreen | Capteurs/actionneurs | Cartes capteurs (temp, hygro, CO₂, UVB), contrôles actionneurs (relais, ventil, brumisation), courbes mini | Tap sur carte -> détail, sliders seuils, toggles modes auto/manu | Très fréquent |
| ui_ProfilesScreen | Profils/Scénarios | Liste profils, boutons Dupliquer/Activer, calendrier/planning simple | Click, VALUE_CHANGED sur listes/checkbox, modale confirmation | Fréquent |
| ui_CommScreen | CAN/RS485 | Dropdown baudrate, toggles modes, liste trames récentes, compteurs | VALUE_CHANGED sur dropdown, boutons Clear/Start, logs auto | Occasionnel |
| ui_StorageScreen | microSD | Statut montage, barre usage, liste fichiers (placeholder), boutons Refresh/Unmount | Click, VALUE_CHANGED sur liste, modale confirmation unmount | Occasionnel |
| ui_DiagnosticsScreen | Système/Logs | Labels CPU/RAM/PSRAM, uptime, versions, chart mini CPU, list logs | Swipe onglets (Stats/Logs), refresh timer, bouton Export logs | Occasionnel |
| ui_TouchTestScreen | Test tactile | Zone cible, affichage coordonnées, bouton Valider | LV_EVENT_PRESSED/RELEASED, display coords, reset test | Rare/maintenance |
| ui_AboutScreen | Infos produit | Logo, version firmware/LVGL/IDF, liens support | Simple click, scroll | Occasionnel |

### Flux utilisateurs typiques
1. **Connexion Wi-Fi initiale** : Home → MenuDrawer → Réseau → Scan (LV_EVENT_CLICKED) → sélection SSID (VALUE_CHANGED) → textarea SSID/pwd focus → clavier AZERTY apparaît → bouton Connect (CLICKED) appelle `hw_network_connect()` → mise à jour StatusBar.
2. **Réglage luminosité & mode nuit** : Home → MenuDrawer → Système → slider `ui_sliderBacklight` (VALUE_CHANGED) → appelle `hw_backlight_set_level()` + `app_ui_update_status_bar()` → bouton preset “Nuit” applique valeur basse et thème sombre.
3. **Diagnostic capteurs/communications** : Home carte alerte → click -> Diagnostics onglet Logs → affichage compteurs CAN/RS485, boutons Clear/Refresh → `hw_comm_set_can_baudrate()` ou `hw_comm_set_rs485_baudrate()` via dropdown event.

## 3) Projet SquareLine Studio 1.5.4 – paramètres
- **Résolution** : 1024×600 paysage.
- **Color depth** : 16 bits RGB565 (équilibre qualité/RAM pour framebuffer RGB + PSRAM autorisée).
- **Buffers LVGL** : Double buffer 20–25% écran en PSRAM recommandé (dma-friendly). Ajuster à 1/5 écran si RAM serrée.
- **Version LVGL** : 9.2 (API v9 stricte, LV_USE_EVDEV off, LV_USE_FREETYPE off par défaut).
- **Écrans à créer** : cf. liste section 2. Chaque écran avec conteneur racine full-screen + `ui_StatusBar` et éventuellement `ui_MenuDrawer` superposés (layers SquareLine) pour cohérence.

### Arborescences de widgets (extraits clés)
- `ui_SplashScreen`: root → col center → Image logo (`ui_imgLogo`) + progress bar (`ui_barBoot`) + label état (`ui_lblBootState`).
- `ui_HomeScreen`: root → StatusBar overlay → row top cards (`ui_panelMainCards`) avec cartes capteurs (temp/hygro/CO₂/UVB), row actions rapides (boutons `ui_btnClimate`, `ui_btnProfiles`, `ui_btnDiag`), onglets (`ui_tabviewHome`) avec mini-graphes et logs récents.
- `ui_MenuDrawer`: root overlay → panel latéral (`ui_panelDrawer`) contenant avatar, label profil, liste boutons (`ui_btnNavHome`, `ui_btnNavNetwork`, etc.), bouton paramètres rapides (`ui_btnLock`).
- `ui_NetworkScreen`: root → StatusBar → col → row scan/connect → list SSID (`ui_listWifi`) → textareas `ui_taSsid`, `ui_taPwd` + boutons `ui_btnScan`, `ui_btnConnect`, `ui_btnDisconnect`, labels état IP/RSSI, toggle BLE `ui_swBle`.
- `ui_SystemScreen`: root → StatusBar → col → sliders `ui_sliderBacklight`, `ui_sliderBrightness` (thème) → boutons presets (`ui_btnPresetDay`, `ui_btnPresetNight`, `ui_btnPresetIndoor`) → réglage date/heure (fields + clavier), langue dropdown `ui_ddLang`.
- `ui_ClimateScreen`: root → StatusBar → grid cartes capteurs `ui_panelClimateCards` → toggles actionneurs (`ui_swFogger`, `ui_swHeater`, `ui_swVent`) → sliders seuils (`ui_sliderTempSet`, `ui_sliderHumiditySet`) → bouton mode Auto/Manu `ui_btnClimateMode` → onglet graphes `ui_tabviewClimate`.
- `ui_ProfilesScreen`: root → StatusBar → list profils `ui_listProfiles` → boutons `ui_btnProfileNew`, `ui_btnProfileDuplicate`, `ui_btnProfileActivate` → planning simple (list/time selectors) → modale confirmation `ui_modalProfileConfirm`.
- `ui_CommScreen`: root → StatusBar → dropdown baud CAN `ui_ddCanBaud`, dropdown RS485 `ui_ddRs485Baud`, toggle modes `ui_swCanListen`, `ui_swRs485Dir` → list trames reçues `ui_listFrames` → compteurs labels → boutons `ui_btnCommClear`, `ui_btnCommStart`.
- `ui_StorageScreen`: root → StatusBar → panel statut SD (`ui_lblSdStatus`, `ui_barSdUsage`) → liste fichiers placeholder `ui_listFiles` → boutons `ui_btnSdRefresh`, `ui_btnSdUnmount`.
- `ui_DiagnosticsScreen`: root → StatusBar → tabview Stats/Logs (`ui_tabDiag`) → Stats: labels CPU/RAM/PSRAM (`ui_lblCpuFreq`, `ui_lblRam`, `ui_lblPsram`), chart mini CPU `ui_chartCpu`; Logs: list `ui_listLogs`, bouton `ui_btnExportLogs`.
- `ui_TouchTestScreen`: root → StatusBar → canvas zone test `ui_canvasTouch` + cibles, label coords `ui_lblTouchCoords`, bouton `ui_btnTouchValidate`.
- `ui_AboutScreen`: root → StatusBar → logo, labels versions (`ui_lblFwVersion`, `ui_lblLvglVersion`, `ui_lblIdfVersion`), lien support/bouton `ui_btnSupport`.

### Conventions de nommage (préfixe `ui_`)
- Labels valeurs : `ui_lblTempValue`, `ui_lblWifiStatus`
- Boutons : `ui_btnConnect`, `ui_btnSave`, `ui_btnBack`
- Sliders : `ui_sliderBrightness`, `ui_sliderBacklight`
- Switches : `ui_swBle`, `ui_swFogger`
- Dropdowns/Listes : `ui_ddCanBaud`, `ui_listLogs`
- Textareas : `ui_taSsid`, `ui_taPwd`
- Styles réutilisables : `style_btn_primary`, `style_panel_card`, `style_label_value`, `style_chip_status`

### Styles, palette, typographies
- Palette (contraste élevé, usage terrain) :
  - Fond principal : #0B1526
  - Panels/cards : #162235 / #1E2F45
  - Accent primaire : #4FC3F7
  - Accent succès : #4CAF50
  - Alerte : #FF7043
  - Texte principal : #E0E6F0 ; secondaire : #9FB2CC
- Typographies :
  - Titres (24–28 px), Sous-titres (20–22 px), Valeurs (26–32 px), Légendes (16–18 px).
- Styles SquareLine :
  - `style_btn_primary` : rayon 8, padding 12–16, couleur primaire, texte blanc.
  - `style_btn_secondary` : fond panel, bord 1px #4FC3F7, texte clair.
  - `style_panel_card` : rayon 12, ombre légère, padding 12, fond panel.
  - `style_label_value` : fonte medium, couleur texte principale.
  - Mutualiser au max (4–6 styles) pour limiter RAM; associer via classes SquareLine, pas de styles individuels multiples.

### Gestion des événements LVGL 9.2
- Événements clés : `LV_EVENT_CLICKED`, `LV_EVENT_PRESSED`, `LV_EVENT_RELEASED`, `LV_EVENT_VALUE_CHANGED`, `LV_EVENT_READY`, `LV_EVENT_CANCEL`, `LV_EVENT_FOCUSED`, `LV_EVENT_DEFOCUSED`.
- Convention callbacks générés : `ui_event_btnConnect()`, `ui_event_sliderBacklight()`, `ui_event_ddCanBaud()`, `ui_event_swFogger()`, etc.
- Appels logique métier (fichiers app_hw.c / app_logic.c) :
  - Réseau : `hw_network_connect(ssid, pwd)`, `hw_network_disconnect()`, `app_ui_update_network_status(state)`
  - Backlight : `hw_backlight_set_level(uint8_t level)`
  - SD : `hw_sdcard_refresh_status()`, `hw_sdcard_is_mounted()`
  - Comm : `hw_comm_set_can_baudrate(uint32_t baud)`, `hw_comm_set_rs485_baudrate(uint32_t baud)`
  - Diag : `hw_diag_get_stats(&stats)`
- Exemple : `ui_event_btnConnect()` → lit textareas → appelle `hw_network_connect()` → affiche modale progression → sur succès/échec met à jour labels via `app_ui_update_status_bar()`.

## 4) Intégration hardware Waveshare (UI)
- **StatusBar** : icônes Wi-Fi (état + RSSI), BLE, SD montée, alerte (logs récents), luminosité (niveau backlight), horloge RTC.
- **Réseau** : scan Wi-Fi (bouton), liste SSID, textareas SSID/PWD (clavier AZERTY), boutons Connect/Disconnect, labels IP/RSSI; toggle BLE placeholder.
- **Stockage microSD** : label statut (monté/erreur), barre utilisation, liste fichiers placeholder, boutons Refresh/Unmount (modale confirmation), appel `hw_sdcard_refresh_status()` dans `ui_event_btnSdRefresh()`.
- **Communication CAN/RS485** : dropdowns baudrate, toggles mode, compteurs trames, liste frames reçues; callbacks `ui_event_ddCanBaud()` → `hw_comm_set_can_baudrate()`, `ui_event_ddRs485Baud()` → `hw_comm_set_rs485_baudrate()`.
- **Diagnostics** : affichage stats `hw_diag_stats_t` (CPU, RAM/PSRAM, uptime, versions), chart CPU, logs list; bouton Export déclenche placeholder.
- **Test tactile** : canvas avec cibles, affichage coordonnées de `lv_event_get_point()`, bouton Valider pour marquer test OK.
- **Réglage backlight** : slider + boutons presets appellent `hw_backlight_set_level()` et `app_ui_update_status_bar()`; mode jour/nuit applique styles globaux.

## 5) Intégration ESP-IDF + LVGL 9.2 + SquareLine (squelette)
Voir fichiers `main/app_main.c`, `main/ui_init.c`, `main/ui_events.c`, `main/app_hw.h|c`, `main/CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`.

## 6) Clavier virtuel AZERTY
- **Widgets** : `lv_keyboard` (`ui_kbAzerty`), `lv_textarea` associés (SSID, PWD, noms profils). SquareLine crée keyboard + textareas; mapping AZERTY personnalisé ajouté en C.
- **Logique** : 
  - Focus sur textarea → `LV_EVENT_FOCUSED` : montrer clavier (set parent visible, attach to textarea via `lv_keyboard_set_textarea`).
  - Bouton OK (LV_EVENT_READY) ou Cancel (LV_EVENT_CANCEL) → masquer clavier, clear focus.
- **Personnalisation layout** : SquareLine ne gère pas AZERTY natif → ajouter en C : `static const char *kb_map_azerty[] = {...};` puis `lv_keyboard_set_map(ui_kbAzerty, LV_KEYBOARD_MODE_TEXT_LOWER, kb_map_azerty, kb_ctrl_azerty);` lors de `ui_init_custom()`.
- **Callbacks** :
  - `ui_event_taSsid()/ui_event_taPwd()` : afficher clavier.
  - `ui_event_kbOk()` : masquer clavier, valider texte.
  - `ui_event_kbCancel()` : masquer sans valider.

## 7) Qualité et livrable
- Noms d’écrans, widgets, callbacks cohérents (préfixe `ui_`).
- Code C compatible ESP-IDF 6.x, LVGL 9.2, CMake moderne.
- Styles mutualisés, buffers LVGL dimensionnés, PSRAM recommandé.
- TODO explicites pour drivers display/tactile et logique métier.

