#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REPTILE_MAX_NAME_LEN      32
#define REPTILE_MAX_SPECIES_LEN   32
#define REPTILE_MAX_DESC_LEN      96
#define REPTILE_MAX_LOG_ENTRIES   100
#define REPTILE_MAX_REPTILES      50

#define REPTILE_CORE_DATA_FILENAME "reptiles_data.json"
#define REPTILE_AUDIT_LOG_FILENAME "audit.log"

typedef enum {
    REPTILE_EVENT_FEEDING,
    REPTILE_EVENT_SHEDDING,
    REPTILE_EVENT_HEALTH,
    REPTILE_EVENT_MAINTENANCE,
} reptile_event_type_t;

typedef struct {
    char date[20];
    char description[REPTILE_MAX_DESC_LEN];
} reptile_log_entry_t;

typedef struct {
    char name[REPTILE_MAX_NAME_LEN];
    char species[REPTILE_MAX_SPECIES_LEN];
    float weight;
    float target_temp;
    bool healthy;

    reptile_log_entry_t feedings[REPTILE_MAX_LOG_ENTRIES];
    int feedings_count;
    reptile_log_entry_t sheddings[REPTILE_MAX_LOG_ENTRIES];
    int sheddings_count;
    reptile_log_entry_t health_checks[REPTILE_MAX_LOG_ENTRIES];
    int health_count;
    reptile_log_entry_t maintenances[REPTILE_MAX_LOG_ENTRIES];
    int maint_count;
} reptile_t;

typedef struct {
    reptile_t reptiles[REPTILE_MAX_REPTILES];
    int reptile_count;
} reptiles_data_t;

/**
 * @brief Initialise la structure en mémoire puis tente de charger depuis le fichier JSON.
 *        En absence de fichier ou en cas d'erreur, la structure est réinitialisée proprement.
 */
esp_err_t reptile_core_init(reptiles_data_t *data, const char *mount_point);

/**
 * @brief Sauvegarde la structure en JSON dans le point de montage fourni.
 */
esp_err_t reptile_core_save(const reptiles_data_t *data, const char *mount_point);

/**
 * @brief Ajoute un reptile si la capacité n'est pas dépassée.
 */
esp_err_t reptile_core_add_reptile(reptiles_data_t *data, const reptile_t *reptile);

/**
 * @brief Ajoute un événement typé pour un reptile donné et horodate automatiquement.
 */
esp_err_t reptile_core_record_event(reptile_t *reptile, reptile_event_type_t type, const char *description);

/**
 * @brief Écrit une entrée d'audit textuelle (date incluse) dans le fichier de log.
 */
esp_err_t reptile_core_audit_append(const char *mount_point, const char *action);

#ifdef __cplusplus
}
#endif

