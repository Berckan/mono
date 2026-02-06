/**
 * System Info Implementation
 *
 * Reads battery and volume information from Trimui Brick hardware.
 * Battery: sysfs interface at /sys/class/power_supply/axp2202-battery/
 * Volume: ALSA mixer "DAC volume" (range 0-255)
 *
 * Uses time-based caching to avoid excessive system calls:
 * - Volume: refreshed every 500ms (responds quickly to changes)
 * - Battery: refreshed every 10 seconds (changes slowly)
 */

#include "sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>

// Sysfs paths for battery on Trimui Brick (AXP2202 PMIC)
#define BATTERY_CAPACITY_PATH "/sys/class/power_supply/axp2202-battery/capacity"
#define BATTERY_STATUS_PATH "/sys/class/power_supply/axp2202-battery/status"

// Cache refresh intervals (milliseconds)
#define VOLUME_REFRESH_MS 100     // Refresh volume every 100ms (responsive to hardware changes)
#define BATTERY_REFRESH_MS 10000  // Refresh battery every 10 seconds

// Cached values
static int g_cached_battery = -1;
static int g_cached_volume = -1;
static BatteryStatus g_cached_status = BATTERY_UNKNOWN;

// Last refresh timestamps (milliseconds)
static unsigned long g_last_volume_refresh = 0;
static unsigned long g_last_battery_refresh = 0;

/**
 * Get current time in milliseconds
 */
static unsigned long get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/**
 * Read an integer value from a sysfs file
 */
static int read_sysfs_int(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    int value = -1;
    if (fscanf(f, "%d", &value) != 1) {
        value = -1;
    }
    fclose(f);
    return value;
}

/**
 * Read a string from a sysfs file
 */
static int read_sysfs_string(const char *path, char *buf, size_t size) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (!fgets(buf, size, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    // Remove trailing newline
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }
    return 0;
}

/**
 * Parse volume from amixer output
 * Uses 'digital volume' which is the system volume control on Trimui Brick
 * Note: This control is inverted (0% = max volume, 100% = mute)
 * Example output: "  Mono: 3 [5%]"
 */
static int parse_amixer_volume(void) {
    // Run amixer to get digital volume (system volume on Trimui Brick)
    FILE *pipe = popen("amixer get 'digital volume' 2>/dev/null", "r");
    if (!pipe) return -1;

    char line[256];
    int volume = -1;

    while (fgets(line, sizeof(line), pipe)) {
        // Look for line with percentage in brackets
        char *bracket = strstr(line, "[");
        if (bracket) {
            int pct;
            if (sscanf(bracket, "[%d%%]", &pct) == 1) {
                // Invert: digital volume is inverted (0% = loud, 100% = mute)
                volume = 100 - pct;
                break;
            }
        }
    }

    pclose(pipe);
    return volume;
}

/**
 * Refresh battery cache if expired
 */
static void refresh_battery_if_needed(void) {
    unsigned long now = get_time_ms();
    if (now - g_last_battery_refresh >= BATTERY_REFRESH_MS || g_last_battery_refresh == 0) {
#ifdef __APPLE__
        g_cached_battery = 73;
        g_cached_status = BATTERY_DISCHARGING;
#else
        int value = read_sysfs_int(BATTERY_CAPACITY_PATH);
        if (value >= 0) {
            g_cached_battery = value;
        }

        // Also refresh status
        char status[32];
        if (read_sysfs_string(BATTERY_STATUS_PATH, status, sizeof(status)) == 0) {
            if (strcmp(status, "Charging") == 0) {
                g_cached_status = BATTERY_CHARGING;
            } else if (strcmp(status, "Full") == 0) {
                g_cached_status = BATTERY_FULL;
            } else if (strcmp(status, "Discharging") == 0 || strcmp(status, "Not charging") == 0) {
                g_cached_status = BATTERY_DISCHARGING;
            } else {
                g_cached_status = BATTERY_UNKNOWN;
            }
        }
#endif
        g_last_battery_refresh = now;
    }
}

/**
 * Refresh volume cache if expired
 */
static void refresh_volume_if_needed(void) {
    unsigned long now = get_time_ms();
    if (now - g_last_volume_refresh >= VOLUME_REFRESH_MS || g_last_volume_refresh == 0) {
#ifdef __APPLE__
        g_cached_volume = 65;
#else
        int value = parse_amixer_volume();
        if (value >= 0) {
            g_cached_volume = value;
        }
#endif
        g_last_volume_refresh = now;
    }
}

int sysinfo_init(void) {
    // Force initial refresh
    g_last_battery_refresh = 0;
    g_last_volume_refresh = 0;
    refresh_battery_if_needed();
    refresh_volume_if_needed();

    // Success even if values unavailable (for desktop testing)
    return 0;
}

int sysinfo_get_battery_percent(void) {
    refresh_battery_if_needed();
    return g_cached_battery;
}

BatteryStatus sysinfo_get_battery_status(void) {
    refresh_battery_if_needed();
    return g_cached_status;
}

bool sysinfo_is_charging(void) {
    BatteryStatus status = sysinfo_get_battery_status();
    return (status == BATTERY_CHARGING || status == BATTERY_FULL);
}

int sysinfo_get_volume(void) {
    refresh_volume_if_needed();
    return g_cached_volume;
}

void sysinfo_refresh_volume(void) {
    // Force immediate refresh by resetting timestamp
    g_last_volume_refresh = 0;
    refresh_volume_if_needed();
}

void sysinfo_cleanup(void) {
    // Nothing to cleanup for sysfs/amixer
}
