#include "system_status.h"

#include <string.h>

static system_status_t s_status = {0};
static SemaphoreHandle_t s_status_lock = NULL;

static inline SemaphoreHandle_t get_lock(void)
{
    if (s_status_lock == NULL)
    {
        s_status_lock = xSemaphoreCreateMutex();
    }
    return s_status_lock;
}

static inline void with_lock(void (*fn)(void *), void *arg)
{
    SemaphoreHandle_t lock = get_lock();
    if (lock == NULL)
    {
        return;
    }

    if (xSemaphoreTake(lock, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        fn(arg);
        xSemaphoreGive(lock);
    }
}

void system_status_init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    get_lock();
}

static void set_sd_mounted_locked(void *arg)
{
    s_status.sd_mounted = (arg != NULL);
}

void system_status_set_sd_mounted(bool mounted)
{
    with_lock(set_sd_mounted_locked, mounted ? (void *)1 : NULL);
}

static void set_touch_available_locked(void *arg)
{
    s_status.touch_available = (arg != NULL);
}

void system_status_set_touch_available(bool available)
{
    with_lock(set_touch_available_locked, available ? (void *)1 : NULL);
}

static void set_wifi_locked(void *arg)
{
    const char *ssid = (const char *)arg;
    s_status.wifi_connected = (ssid != NULL);
    if (ssid)
    {
        strlcpy(s_status.wifi_ssid, ssid, sizeof(s_status.wifi_ssid));
    }
    else
    {
        s_status.wifi_ssid[0] = '\0';
    }
}

void system_status_set_wifi(bool connected, const char *ssid)
{
    const char *safe_ssid = (connected && ssid) ? ssid : NULL;
    with_lock(set_wifi_locked, (void *)safe_ssid);
}

static void copy_status_locked(void *arg)
{
    if (arg)
    {
        memcpy(arg, &s_status, sizeof(s_status));
    }
}

void system_status_get(system_status_t *out_status)
{
    if (out_status == NULL)
    {
        return;
    }
    with_lock(copy_status_locked, out_status);
}

const system_status_t *system_status_get_ref(void)
{
    return &s_status;
}

