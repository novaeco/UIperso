# Tâches restantes

Il n’existe plus de backlog ouvert pour la demande principale. Les items ci-dessous rappellent simplement ce qui a déjà été livré et où le vérifier :

- **UI SquareLine/LVGL générée** : tous les écrans et callbacks décrits dans `docs/UI_SPEC.md` sont implémentés dans `main/ui_init.c` et initiauxés via `ui_init()`. Vérifications détaillées en section 8 de `docs/UI_SPEC.md`.
- **Intégration matérielle** : les stubs complétés (backlight LEDC, Wi‑Fi STA, microSD, CAN/RS485, diagnostics, test tactile) sont dans `main/app_hw.c` ; les appels sont reliés aux événements UI.
- **Clavier AZERTY étendu** : mapping complet et gestion d’apparition/masquage via `ui_kbAzerty` et les callbacks associés.
- **Config build** : buffers LVGL PSRAM, sdkconfig orienté RGB/LVGL/Wi‑Fi/TWAI et table de partitions OTA sont déjà fournis.

Si un écart fonctionnel est constaté, il doit être ouvert comme nouveau ticket ; à ce stade, aucune action planifiée ne reste à exécuter.
