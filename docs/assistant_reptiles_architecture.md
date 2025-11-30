# Assistant Administratif Reptiles — Architecture embarquée

## Vue d'ensemble
Ce document décrit la base logicielle créée pour transformer UIperso en un assistant embarqué ESP32-S3 dédié à la gestion d'élevage de reptiles. L'accent est mis sur des modules séparés par responsabilités : logique métier (reptiles, événements, audit) et connectivité (Wi-Fi + NTP + préférences).

## Modules ajoutés

### `components/reptile_core`
- **Structures métier** : `reptile_t`, `reptiles_data_t` avec capacités configurables (50 reptiles, 100 entrées par journal).
- **Persistance JSON** : `reptile_core_save()` et `reptile_core_init()` sérialisent/désérialisent `reptiles_data_t` vers/depuis `/sdcard/reptiles_data.json` (chemin paramétrable). Les lectures gèrent les erreurs et repartent d'un état vierge en cas de corruption ou d'absence de fichier.
- **Journal d'événements** : `reptile_core_record_event()` horodate automatiquement (format `YYYY-MM-DD HH:MM`) et ajoute l'événement dans le bon tableau (repas, mue, santé, maintenance) avec gestion de capacité.
- **Audit append-only** : `reptile_core_audit_append()` écrit un log texte horodaté (`audit.log`) pour tracer les actions utilisateur sans bloquer l'application.

### `components/reptile_net`
- **Stockage des identifiants Wi-Fi en NVS** : `reptile_net_prefs_init()`, `reptile_net_prefs_save_wifi()`, `reptile_net_prefs_load_wifi()` encapsulent l'accès NVS (namespace `wifi`) pour conserver SSID/mot de passe sans les durcir dans le firmware.
- **Wi-Fi station robuste** : `reptile_net_wifi_start()` instancie l'interface STA, enregistre les handlers d'événements, limite les tentatives de reconnexion et expose l'IP obtenue via `reptile_wifi_state_t`.
- **Synchronisation NTP** : `reptile_net_ntp_start()` initialise SNTP, applique un fuseau horaire configurable et laisse l'horloge système synchroniser les horodatages utilisés par les journaux métier.

## Intégration
- Le composant `main` dépend désormais explicitement de `reptile_core` et `reptile_net` (voir `main/CMakeLists.txt`), garantissant que les modules sont construits avec le reste du firmware.
- Les points de montage restent paramétrables (ex. `/sdcard`) ; en absence de stockage ou de réseau, les fonctions retournent un `esp_err_t` explicite sans bloquer l'UI.

## Prochaines étapes
- Brancher le gestionnaire UI (`ui_manager`) sur `reptile_core` pour afficher/éditer les reptiles et déclencher les sauvegardes JSON après chaque action.
- Ajouter un panneau Paramètres exploitant `reptile_net_prefs_*` et `reptile_net_wifi_start()` avec clavier virtuel LVGL pour saisir les identifiants.
- Créer des tâches dédiées pour NTP et pour la persistance périodique afin d'éviter toute charge sur la tâche LVGL, conformément aux règles RTOS du projet.
