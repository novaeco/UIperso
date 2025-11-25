#include "cs8501.h"

#include "esp_log.h"

#include <math.h>
#include <string.h>

static const char *TAG = "CS8501";

static bool s_voltage_available = false;
static bool s_charge_status_available = false;

static void log_unavailable_once(const char *what)
{
    static bool warned_voltage = false;
    static bool warned_charge = false;

    bool *warned = (what == NULL) ? NULL : (strcmp(what, "voltage") == 0 ? &warned_voltage : &warned_charge);
    if (warned && !(*warned))
    {
        *warned = true;
        ESP_LOGW(TAG, "%s telemetry unavailable on this hardware revision; returning NAN/false", what);
    }
}

void cs8501_init(void)
{
    // Hardware integration for CS8501 (battery gauge / charger) is not routed
    // to the ESP32-S3 MCU on this revision. Keep the driver non-fatal while
    // exposing capability flags so the UI can degrade gracefully.
    s_voltage_available = false;
    s_charge_status_available = false;
    ESP_LOGI(TAG, "CS8501 power controller initialized (telemetry unavailable)");
}

float cs8501_get_battery_voltage(void)
{
    if (!s_voltage_available)
    {
        log_unavailable_once("voltage");
        return NAN;
    }

    return NAN;
}

bool cs8501_is_charging(void)
{
    if (!s_charge_status_available)
    {
        log_unavailable_once("charge");
        return false;
    }

    return false;
}

bool cs8501_has_voltage_reading(void)
{
    return s_voltage_available;
}

bool cs8501_has_charge_status(void)
{
    return s_charge_status_available;
}
