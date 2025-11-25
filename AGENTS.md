# AGENTS.md — Instructions globales (UIperso)

## Objectif
Tu es un agent de contribution code (Codex). Ta mission: livrer des changements production-ready, compilables ESP-IDF 6.1, stables au boot, sans régression LCD RGB 1024×600 + GT911 + CH422G + (optionnel) microSD.

## Stack
- Cible: ESP32-S3
- Framework: ESP-IDF 6.1
- UI: LVGL 9.4 (déjà intégré via l’environnement IDF du projet; ne pas télécharger LVGL)
- Dossiers clés:
  - main/ (app + UI)
  - components/ (drivers & services)
  - ui_assets/ (icônes/fonts/themes)

## Règles non négociables
1) Ne jamais inventer pinout/timings/adresses: source de vérité = code du repo.
2) Ne pas “fixer” en désactivant LCD/touch/LVGL.
3) Éviter les changements massifs non demandés (refacto large, renommages).
4) Pas de secrets en dur (SSID/mots de passe/tokens).

## Commandes standard (Windows PowerShell)
Build propre:
- idf.py set-target esp32s3
- idf.py fullclean
- idf.py build

Flash + monitor:
- idf.py -p COMx flash
- idf.py -p COMx monitor

Si incohérence (checksum mismatch / binaire non aligné):
- idf.py -p COMx erase_flash
- idf.py -p COMx flash monitor

## Critères d’acceptation
- `idf.py fullclean build` OK
- Boot stable (pas de reset loop)
- UI LVGL visible, app responsive
- Touch GT911 init OK, événements cohérents
- Sans microSD: aucun crash, fallback propre

## Format livrable
- Résumé cause racine + approche
- Liste des fichiers modifiés
- Patch/diff ou fichiers complets modifiés
- Logs boot utiles si init/tick/LCD/touch/SD modifiés
