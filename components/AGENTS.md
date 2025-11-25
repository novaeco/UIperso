# components/AGENTS.md — Règles composants (drivers/services)

## API / erreurs
- API publique claire (headers), retours esp_err_t.
- Pas d’allocations fréquentes dans callbacks/ISR.

## Concurrence
- Bus partagés (I2C/SPI): mutex si multi-clients.
- Pas d’appels bloquants en ISR.

## CMake / dépendances
- SRCS/INCLUDE_DIRS/REQUIRES corrects.
- Ne pas télécharger LVGL (déjà fourni par l’environnement du projet).

## DoD
- Build OK
- Pas de régression sur I2C/SPI/LCD/Touch/SD
