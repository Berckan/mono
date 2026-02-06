/**
 * System Info - Battery and Volume monitoring for Trimui Brick
 */

#ifndef SYSINFO_H
#define SYSINFO_H

#include <stdbool.h>

/**
 * Battery status
 */
typedef enum {
    BATTERY_DISCHARGING,
    BATTERY_CHARGING,
    BATTERY_FULL,
    BATTERY_UNKNOWN
} BatteryStatus;

/**
 * Initialize system info module
 * @return 0 on success, -1 on failure
 */
int sysinfo_init(void);

/**
 * Get battery percentage (0-100)
 * @return Battery percentage or -1 if unavailable
 */
int sysinfo_get_battery_percent(void);

/**
 * Get battery charging status
 * @return BatteryStatus enum value
 */
BatteryStatus sysinfo_get_battery_status(void);

/**
 * Check if device is charging
 * @return true if charging
 */
bool sysinfo_is_charging(void);

/**
 * Get system volume percentage (0-100)
 * @return Volume percentage or -1 if unavailable
 */
int sysinfo_get_volume(void);

/**
 * Force immediate refresh of volume cache
 * Call this after volume change events for instant update
 */
void sysinfo_refresh_volume(void);

/**
 * Check if WiFi is connected (wlan0 operstate == "up")
 */
bool sysinfo_is_wifi_connected(void);

/**
 * Check if a Bluetooth audio device is connected
 */
bool sysinfo_is_bluetooth_connected(void);

/**
 * Cleanup system info module
 */
void sysinfo_cleanup(void);

#endif // SYSINFO_H
