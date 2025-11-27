#include "system_status.h"

#include <math.h>
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
    s_status.vbat = NAN;
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

static void set_can_ok_locked(void *arg)
{
    s_status.can_ok = (arg != NULL);
}

void system_status_set_can_ok(bool ok)
{
    with_lock(set_can_ok_locked, ok ? (void *)1 : NULL);
}

static void increment_can_frames_locked(void *arg)
{
    (void)arg;
    s_status.can_frames_rx++;
}

void system_status_increment_can_frames(void)
{
    with_lock(increment_can_frames_locked, NULL);
}

static void set_rs485_ok_locked(void *arg)
{
    s_status.rs485_ok = (arg != NULL);
}

void system_status_set_rs485_ok(bool ok)
{
    with_lock(set_rs485_ok_locked, ok ? (void *)1 : NULL);
}

typedef struct
{
    bool tx;
    size_t bytes;
} rs485_count_args_t;

static void update_rs485_counts_locked(void *arg)
{
    rs485_count_args_t *params = (rs485_count_args_t *)arg;
    if (params == NULL)
    {
        return;
    }
    if (params->tx)
    {
        s_status.rs485_tx_count += params->bytes;
    }
    else
    {
        s_status.rs485_rx_count += params->bytes;
    }
}

void system_status_add_rs485_tx(size_t bytes)
{
    rs485_count_args_t params = {.tx = true, .bytes = bytes};
    with_lock(update_rs485_counts_locked, &params);
}

void system_status_add_rs485_rx(size_t bytes)
{
    rs485_count_args_t params = {.tx = false, .bytes = bytes};
    with_lock(update_rs485_counts_locked, &params);
}

typedef struct
{
    bool ok;
    bool telemetry_available;
    float vbat;
    bool charging_known;
    bool charging;
} power_args_t;

static void set_power_locked(void *arg)
{
    power_args_t *params = (power_args_t *)arg;
    if (params == NULL)
    {
        return;
    }

    s_status.power_ok = params->ok;
    s_status.power_telemetry_available = params->telemetry_available;
    s_status.vbat = params->vbat;
    s_status.charging_known = params->charging_known;
    s_status.charging = params->charging;
}

void system_status_set_power(bool ok, bool telemetry_available, float vbat, bool charging_known, bool charging)
{
    power_args_t params = {
        .ok = ok,
        .telemetry_available = telemetry_available,
        .vbat = vbat,
        .charging_known = charging_known,
        .charging = charging,
    };
    with_lock(set_power_locked, &params);
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

