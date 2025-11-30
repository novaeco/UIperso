#include "reptile_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "REPTILE_CORE";

static void fill_current_datetime(char *dst, size_t dst_len)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(dst, dst_len, "%Y-%m-%d %H:%M", &timeinfo);
}

static void reset_data(reptiles_data_t *data)
{
    if (data)
    {
        memset(data, 0, sizeof(*data));
    }
}

static cJSON *reptile_to_json(const reptile_t *rep)
{
    cJSON *jrep = cJSON_CreateObject();
    cJSON_AddStringToObject(jrep, "name", rep->name);
    cJSON_AddStringToObject(jrep, "species", rep->species);
    cJSON_AddNumberToObject(jrep, "weight", rep->weight);
    cJSON_AddNumberToObject(jrep, "target_temp", rep->target_temp);
    cJSON_AddBoolToObject(jrep, "healthy", rep->healthy);

    cJSON *jfeed = cJSON_AddArrayToObject(jrep, "feedings");
    for (int i = 0; i < rep->feedings_count; ++i)
    {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "date", rep->feedings[i].date);
        cJSON_AddStringToObject(entry, "description", rep->feedings[i].description);
        cJSON_AddItemToArray(jfeed, entry);
    }

    cJSON *jshed = cJSON_AddArrayToObject(jrep, "sheddings");
    for (int i = 0; i < rep->sheddings_count; ++i)
    {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "date", rep->sheddings[i].date);
        cJSON_AddStringToObject(entry, "description", rep->sheddings[i].description);
        cJSON_AddItemToArray(jshed, entry);
    }

    cJSON *jhealth = cJSON_AddArrayToObject(jrep, "health_checks");
    for (int i = 0; i < rep->health_count; ++i)
    {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "date", rep->health_checks[i].date);
        cJSON_AddStringToObject(entry, "description", rep->health_checks[i].description);
        cJSON_AddItemToArray(jhealth, entry);
    }

    cJSON *jmaint = cJSON_AddArrayToObject(jrep, "maintenances");
    for (int i = 0; i < rep->maint_count; ++i)
    {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "date", rep->maintenances[i].date);
        cJSON_AddStringToObject(entry, "description", rep->maintenances[i].description);
        cJSON_AddItemToArray(jmaint, entry);
    }

    return jrep;
}

static bool json_to_log_array(cJSON *array, reptile_log_entry_t *dst, int *count)
{
    if (!cJSON_IsArray(array) || dst == NULL || count == NULL)
    {
        return false;
    }

    const int elements = cJSON_GetArraySize(array);
    const int cap = REPTILE_MAX_LOG_ENTRIES;
    *count = 0;

    for (int i = 0; i < elements && i < cap; ++i)
    {
        cJSON *entry = cJSON_GetArrayItem(array, i);
        if (!cJSON_IsObject(entry))
        {
            continue;
        }

        cJSON *jdate = cJSON_GetObjectItem(entry, "date");
        cJSON *jdesc = cJSON_GetObjectItem(entry, "description");
        if (cJSON_IsString(jdate) && cJSON_IsString(jdesc))
        {
            strlcpy(dst[*count].date, jdate->valuestring, sizeof(dst[*count].date));
            strlcpy(dst[*count].description, jdesc->valuestring, sizeof(dst[*count].description));
            (*count)++;
        }
    }
    return true;
}

static bool json_to_reptile(cJSON *obj, reptile_t *out)
{
    if (!cJSON_IsObject(obj) || out == NULL)
    {
        return false;
    }

    cJSON *jname = cJSON_GetObjectItem(obj, "name");
    cJSON *jspecies = cJSON_GetObjectItem(obj, "species");
    cJSON *jweight = cJSON_GetObjectItem(obj, "weight");
    cJSON *jtarget = cJSON_GetObjectItem(obj, "target_temp");
    cJSON *jhealthy = cJSON_GetObjectItem(obj, "healthy");

    if (!cJSON_IsString(jname) || !cJSON_IsString(jspecies))
    {
        return false;
    }

    memset(out, 0, sizeof(*out));
    strlcpy(out->name, jname->valuestring, sizeof(out->name));
    strlcpy(out->species, jspecies->valuestring, sizeof(out->species));
    out->weight = cJSON_IsNumber(jweight) ? (float)jweight->valuedouble : 0.0f;
    out->target_temp = cJSON_IsNumber(jtarget) ? (float)jtarget->valuedouble : 0.0f;
    out->healthy = cJSON_IsBool(jhealthy) ? cJSON_IsTrue(jhealthy) : true;

    json_to_log_array(cJSON_GetObjectItem(obj, "feedings"), out->feedings, &out->feedings_count);
    json_to_log_array(cJSON_GetObjectItem(obj, "sheddings"), out->sheddings, &out->sheddings_count);
    json_to_log_array(cJSON_GetObjectItem(obj, "health_checks"), out->health_checks, &out->health_count);
    json_to_log_array(cJSON_GetObjectItem(obj, "maintenances"), out->maintenances, &out->maint_count);

    return true;
}

static esp_err_t ensure_mount_point(const char *mount_point)
{
    if (mount_point == NULL || strlen(mount_point) == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static void build_path(char *dst, size_t dst_len, const char *mount_point, const char *filename)
{
    snprintf(dst, dst_len, "%s/%s", mount_point, filename);
}

esp_err_t reptile_core_init(reptiles_data_t *data, const char *mount_point)
{
    reset_data(data);

    if (ensure_mount_point(mount_point) != ESP_OK)
    {
        ESP_LOGW(TAG, "Mount point invalide, données initialisées à blanc");
        return ESP_ERR_INVALID_ARG;
    }

    char path[128];
    build_path(path, sizeof(path), mount_point, REPTILE_CORE_DATA_FILENAME);

    FILE *f = fopen(path, "r");
    if (!f)
    {
        ESP_LOGW(TAG, "Fichier %s absent, initialisation vierge", path);
        return ESP_OK;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 1024 * 256)
    {
        fclose(f);
        ESP_LOGW(TAG, "Taille de fichier inattendue (%ld), réinitialisation", len);
        return ESP_FAIL;
    }

    char *buffer = malloc(len + 1);
    if (!buffer)
    {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(buffer, 1, len, f);
    buffer[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    if (!root)
    {
        ESP_LOGE(TAG, "JSON invalide, données réinitialisées");
        return ESP_FAIL;
    }

    cJSON *jreps = cJSON_GetObjectItem(root, "reptiles");
    if (!cJSON_IsArray(jreps))
    {
        cJSON_Delete(root);
        ESP_LOGW(TAG, "Pas de tableau 'reptiles' dans le JSON, réinitialisation");
        return ESP_FAIL;
    }

    const int items = cJSON_GetArraySize(jreps);
    for (int i = 0; i < items && i < REPTILE_MAX_REPTILES; ++i)
    {
        cJSON *jrep = cJSON_GetArrayItem(jreps, i);
        if (json_to_reptile(jrep, &data->reptiles[data->reptile_count]))
        {
            data->reptile_count++;
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Chargé %d reptile(s) depuis %s", data->reptile_count, path);
    return ESP_OK;
}

esp_err_t reptile_core_save(const reptiles_data_t *data, const char *mount_point)
{
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t mp_err = ensure_mount_point(mount_point);
    if (mp_err != ESP_OK)
    {
        return mp_err;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *jreptiles = cJSON_AddArrayToObject(root, "reptiles");
    for (int i = 0; i < data->reptile_count && i < REPTILE_MAX_REPTILES; ++i)
    {
        cJSON *jrep = reptile_to_json(&data->reptiles[i]);
        cJSON_AddItemToArray(jreptiles, jrep);
    }

    char *txt = cJSON_Print(root);
    cJSON_Delete(root);
    if (!txt)
    {
        return ESP_ERR_NO_MEM;
    }

    char path[128];
    build_path(path, sizeof(path), mount_point, REPTILE_CORE_DATA_FILENAME);
    FILE *f = fopen(path, "w");
    if (!f)
    {
        cJSON_free(txt);
        return ESP_FAIL;
    }

    const size_t written = fwrite(txt, 1, strlen(txt), f);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    cJSON_free(txt);

    if (written == 0)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Données sauvegardées (%d reptiles) vers %s", data->reptile_count, path);
    return ESP_OK;
}

esp_err_t reptile_core_add_reptile(reptiles_data_t *data, const reptile_t *reptile)
{
    if (data == NULL || reptile == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (data->reptile_count >= REPTILE_MAX_REPTILES)
    {
        return ESP_ERR_NO_MEM;
    }

    data->reptiles[data->reptile_count] = *reptile;
    data->reptile_count++;
    return ESP_OK;
}

static reptile_log_entry_t *select_log_array(reptile_t *reptile, reptile_event_type_t type, int **count)
{
    if (reptile == NULL || count == NULL)
    {
        return NULL;
    }

    switch (type)
    {
    case REPTILE_EVENT_FEEDING:
        *count = &reptile->feedings_count;
        return reptile->feedings;
    case REPTILE_EVENT_SHEDDING:
        *count = &reptile->sheddings_count;
        return reptile->sheddings;
    case REPTILE_EVENT_HEALTH:
        *count = &reptile->health_count;
        return reptile->health_checks;
    case REPTILE_EVENT_MAINTENANCE:
        *count = &reptile->maint_count;
        return reptile->maintenances;
    default:
        return NULL;
    }
}

esp_err_t reptile_core_record_event(reptile_t *reptile, reptile_event_type_t type, const char *description)
{
    if (reptile == NULL || description == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    int *count = NULL;
    reptile_log_entry_t *array = select_log_array(reptile, type, &count);
    if (!array || !count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (*count >= REPTILE_MAX_LOG_ENTRIES)
    {
        return ESP_ERR_NO_MEM;
    }

    reptile_log_entry_t *entry = &array[*count];
    fill_current_datetime(entry->date, sizeof(entry->date));
    strlcpy(entry->description, description, sizeof(entry->description));
    (*count)++;
    return ESP_OK;
}

esp_err_t reptile_core_audit_append(const char *mount_point, const char *action)
{
    if (action == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t mp_err = ensure_mount_point(mount_point);
    if (mp_err != ESP_OK)
    {
        return mp_err;
    }

    char path[128];
    build_path(path, sizeof(path), mount_point, REPTILE_AUDIT_LOG_FILENAME);

    FILE *f = fopen(path, "a");
    if (!f)
    {
        ESP_LOGE(TAG, "Impossible d'ouvrir %s", path);
        return ESP_FAIL;
    }

    char ts[20];
    fill_current_datetime(ts, sizeof(ts));
    fprintf(f, "%s - %s\n", ts, action);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    return ESP_OK;
}

