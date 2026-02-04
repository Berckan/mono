/**
 * Screen Brightness Control Implementation
 *
 * Gradual dimming instead of binary on/off for better UX.
 * Uses sysfs interface for backlight control on Trimui Brick.
 */

#include "screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Backlight sysfs paths (try multiple for compatibility)
static const char *BRIGHTNESS_PATHS[] = {
    "/sys/class/backlight/backlight/brightness",
    "/sys/class/backlight/lcd-backlight/brightness",
    "/sys/devices/platform/backlight/backlight/backlight/brightness",
    NULL
};

static const char *MAX_BRIGHTNESS_PATHS[] = {
    "/sys/class/backlight/backlight/max_brightness",
    "/sys/class/backlight/lcd-backlight/max_brightness",
    "/sys/devices/platform/backlight/backlight/backlight/max_brightness",
    NULL
};

// State
static int g_saved_brightness = -1;
static int g_max_brightness = 255;
static bool g_is_dimmed = false;
static const char *g_brightness_path = NULL;

// Dim to 10% of max brightness
#define DIM_PERCENT 10

/**
 * Find working brightness path
 */
static const char *find_brightness_path(const char *paths[]) {
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (f) {
            fclose(f);
            return paths[i];
        }
    }
    return NULL;
}

/**
 * Read integer value from sysfs file
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
 * Write integer value to sysfs file
 */
static int write_sysfs_int(const char *path, int value) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    int result = fprintf(f, "%d", value);
    fclose(f);
    return (result > 0) ? 0 : -1;
}

int screen_init(void) {
    // Find brightness control path
    g_brightness_path = find_brightness_path(BRIGHTNESS_PATHS);
    if (!g_brightness_path) {
        fprintf(stderr, "[SCREEN] No backlight control found (desktop mode?)\n");
        return -1;
    }

    // Read max brightness
    const char *max_path = find_brightness_path(MAX_BRIGHTNESS_PATHS);
    if (max_path) {
        int max = read_sysfs_int(max_path);
        if (max > 0) {
            g_max_brightness = max;
        }
    }

    // Save current brightness
    g_saved_brightness = read_sysfs_int(g_brightness_path);
    if (g_saved_brightness < 0) {
        g_saved_brightness = g_max_brightness;
    }

    printf("[SCREEN] Init: path=%s, current=%d, max=%d\n",
           g_brightness_path, g_saved_brightness, g_max_brightness);

    return 0;
}

void screen_dim(void) {
    if (!g_brightness_path || g_is_dimmed) return;

    // Save current if not already saved
    if (g_saved_brightness < 0) {
        g_saved_brightness = read_sysfs_int(g_brightness_path);
    }

    // Calculate 10% of max
    int dim_value = (g_max_brightness * DIM_PERCENT) / 100;
    if (dim_value < 1) dim_value = 1;  // Never fully off

    if (write_sysfs_int(g_brightness_path, dim_value) == 0) {
        g_is_dimmed = true;
        printf("[SCREEN] Dimmed to %d%% (%d)\n", DIM_PERCENT, dim_value);
    }
}

void screen_restore(void) {
    if (!g_brightness_path || !g_is_dimmed) return;

    int restore_value = (g_saved_brightness > 0) ? g_saved_brightness : g_max_brightness;

    if (write_sysfs_int(g_brightness_path, restore_value) == 0) {
        g_is_dimmed = false;
        printf("[SCREEN] Restored to %d\n", restore_value);
    }
}

bool screen_toggle_dim(void) {
    if (g_is_dimmed) {
        screen_restore();
    } else {
        screen_dim();
    }
    return g_is_dimmed;
}

bool screen_is_dimmed(void) {
    return g_is_dimmed;
}

void screen_cleanup(void) {
    if (g_is_dimmed) {
        screen_restore();
    }
}
