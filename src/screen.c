/**
 * Screen Brightness Control Implementation
 *
 * Gradual dimming instead of binary on/off for better UX.
 * Uses /dev/disp ioctl for backlight control on Trimui Brick (Allwinner sunxi).
 */

#include "screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

// Backlight sysfs paths (try multiple for compatibility - used for dim only)
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

// /dev/disp ioctl for backlight control (Allwinner sunxi disp2 driver)
// Trimui Brick uses disp2 commands (0x10x), NOT disp1 (0x14x)
#define DISP_DEV "/dev/disp"
#define DISP2_LCD_SET_BRIGHTNESS 0x102
#define DISP2_LCD_GET_BRIGHTNESS 0x103

// GPIO path for power switch (Trimui Brick)
#define GPIO_SWITCH_PATH "/sys/class/gpio/gpio243/value"

// Framebuffer blank path for display on/off
#define FB_BLANK_PATH "/sys/class/graphics/fb0/blank"

// LED animation sysfs paths (Trimui Brick)
#define LED_ANIM_PATH "/sys/class/led_anim/"
#define LED_MAX_SCALE LED_ANIM_PATH "max_scale"
#define LED_MAX_SCALE_F1F2 LED_ANIM_PATH "max_scale_f1f2"
#define LED_MAX_SCALE_LR LED_ANIM_PATH "max_scale_lr"
#define LED_EFFECT_F1 LED_ANIM_PATH "effect_f1"
#define LED_EFFECT_RGB_F1 LED_ANIM_PATH "effect_rgb_hex_f1"
#define LED_EFFECT_CYCLES_F1 LED_ANIM_PATH "effect_cycles_f1"
#define LED_EFFECT_DURATION_F1 LED_ANIM_PATH "effect_duration_f1"

// Heartbeat: 1 green blink every 10 seconds
#define LED_HEARTBEAT_INTERVAL_MS 10000
#define LED_HEARTBEAT_BLINK_MS 200

// State
static int g_saved_brightness = -1;
static int g_saved_brightness_lcd = 255;  // For /dev/disp control (0-255)
static int g_max_brightness = 255;
static bool g_is_dimmed = false;
static bool g_is_off = false;  // Complete display off (via /dev/disp)
static const char *g_brightness_path = NULL;
static int g_disp_fd = -1;  // File descriptor for /dev/disp

// LED state saved before pocket mode
static int g_saved_led_max_scale = -1;
static int g_saved_led_max_scale_f1f2 = -1;
static int g_saved_led_max_scale_lr = -1;
static bool g_leds_off = false;

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

/**
 * Set LCD brightness via /dev/disp ioctl (disp2 driver)
 * @param brightness 0-255 (0 = off, 255 = full)
 * @return 0 on success, -1 on failure
 */
static int disp_set_brightness(int brightness) {
    if (g_disp_fd < 0) return -1;

    // Clamp value
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;

    // disp2 ioctl format: args[0]=screen, args[1]=value
    unsigned long args[4] = {0, (unsigned long)brightness, 0, 0};
    int ret = ioctl(g_disp_fd, DISP2_LCD_SET_BRIGHTNESS, args);

    if (ret < 0) {
        perror("[SCREEN] ioctl DISP2_LCD_SET_BRIGHTNESS");
        return -1;
    }
    return 0;
}

/**
 * Get current LCD brightness via /dev/disp ioctl (disp2 driver)
 * @return 0-255 on success, -1 on failure
 */
static int disp_get_brightness(void) {
    if (g_disp_fd < 0) return -1;

    unsigned long args[4] = {0, 0, 0, 0};
    int ret = ioctl(g_disp_fd, DISP2_LCD_GET_BRIGHTNESS, args);

    // disp2 returns brightness value as return value
    if (ret < 0) {
        perror("[SCREEN] ioctl DISP2_LCD_GET_BRIGHTNESS");
        return -1;
    }
    return ret;
}

int screen_init(void) {
    // Find brightness control path (for dim functionality)
    g_brightness_path = find_brightness_path(BRIGHTNESS_PATHS);
    if (!g_brightness_path) {
        fprintf(stderr, "[SCREEN] No backlight sysfs found (desktop mode?)\n");
    }

    // Read max brightness
    const char *max_path = find_brightness_path(MAX_BRIGHTNESS_PATHS);
    if (max_path) {
        int max = read_sysfs_int(max_path);
        if (max > 0) {
            g_max_brightness = max;
        }
    }

    // Save current brightness (sysfs)
    if (g_brightness_path) {
        g_saved_brightness = read_sysfs_int(g_brightness_path);
        if (g_saved_brightness < 0) {
            g_saved_brightness = g_max_brightness;
        }
    }

    // Open /dev/disp for real backlight control (Trimui Brick)
    g_disp_fd = open(DISP_DEV, O_RDWR);
    if (g_disp_fd < 0) {
        fprintf(stderr, "[SCREEN] Cannot open %s (expected on desktop)\n", DISP_DEV);
    } else {
        // Get current LCD brightness
        int current = disp_get_brightness();
        if (current >= 0) {
            g_saved_brightness_lcd = current;
        }
        printf("[SCREEN] /dev/disp opened, LCD brightness=%d\n", g_saved_brightness_lcd);
    }

    printf("[SCREEN] Init: sysfs=%s, sysfs_brightness=%d, max=%d\n",
           g_brightness_path ? g_brightness_path : "(none)",
           g_saved_brightness, g_max_brightness);

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
    if (g_is_off) {
        screen_on();
    }
    if (g_is_dimmed) {
        screen_restore();
    }

    // Close /dev/disp
    if (g_disp_fd >= 0) {
        close(g_disp_fd);
        g_disp_fd = -1;
    }
}

/**
 * Write string value to sysfs file
 */
static int write_sysfs_str(const char *path, const char *value) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    int result = fputs(value, f);
    fclose(f);
    return (result >= 0) ? 0 : -1;
}

/**
 * Save current LED state and turn off all LEDs
 */
static void leds_save_and_off(void) {
    if (g_leds_off) return;

    // Save current brightness scales
    g_saved_led_max_scale = read_sysfs_int(LED_MAX_SCALE);
    g_saved_led_max_scale_f1f2 = read_sysfs_int(LED_MAX_SCALE_F1F2);
    g_saved_led_max_scale_lr = read_sysfs_int(LED_MAX_SCALE_LR);

    // Turn off all LEDs
    write_sysfs_int(LED_MAX_SCALE, 0);
    write_sysfs_int(LED_MAX_SCALE_F1F2, 0);
    write_sysfs_int(LED_MAX_SCALE_LR, 0);

    g_leds_off = true;
    printf("[SCREEN] LEDs OFF (pocket mode)\n");
}

/**
 * Restore saved LED state
 */
static void leds_restore(void) {
    if (!g_leds_off) return;

    // Restore brightness scales
    if (g_saved_led_max_scale >= 0)
        write_sysfs_int(LED_MAX_SCALE, g_saved_led_max_scale);
    if (g_saved_led_max_scale_f1f2 >= 0)
        write_sysfs_int(LED_MAX_SCALE_F1F2, g_saved_led_max_scale_f1f2);
    if (g_saved_led_max_scale_lr >= 0)
        write_sysfs_int(LED_MAX_SCALE_LR, g_saved_led_max_scale_lr);

    g_leds_off = false;
    printf("[SCREEN] LEDs restored\n");
}

/**
 * Turn display off by setting brightness to 0 (disp2 driver)
 */
void screen_off(void) {
    if (g_is_off) return;

    if (g_disp_fd < 0) {
        fprintf(stderr, "[SCREEN] Cannot turn off: /dev/disp not open\n");
        return;
    }

    // Save current LCD brightness before turning off
    int current = disp_get_brightness();
    if (current > 0) {
        g_saved_brightness_lcd = current;
    }

    // Turn off LEDs (pocket mode)
    leds_save_and_off();

    // Turn off by setting brightness to 0
    if (disp_set_brightness(0) == 0) {
        g_is_off = true;
        printf("[SCREEN] LCD OFF (brightness=0)\n");
    }
}

/**
 * Turn display back on by restoring brightness (disp2 driver)
 */
void screen_on(void) {
    if (!g_is_off) return;

    if (g_disp_fd < 0) {
        fprintf(stderr, "[SCREEN] Cannot turn on: /dev/disp not open\n");
        return;
    }

    // Restore brightness
    int restore_value = (g_saved_brightness_lcd > 0) ? g_saved_brightness_lcd : 255;

    if (disp_set_brightness(restore_value) == 0) {
        g_is_off = false;
        g_is_dimmed = false;

        // Restore LEDs
        leds_restore();

        printf("[SCREEN] LCD ON (brightness=%d)\n", restore_value);
    }
}

/**
 * Check if display is currently off
 */
bool screen_is_off(void) {
    return g_is_off;
}

/**
 * LED heartbeat for pocket mode: brief green blink on f1 every 10 seconds.
 * Call from main loop. Uses Uint32 ticks (SDL_GetTicks).
 */
void screen_update_led_heartbeat(unsigned int now_ms) {
    if (!g_is_off || !g_leds_off) return;

    static unsigned int last_blink = 0;
    static bool blink_on = false;

    if (blink_on) {
        // Turn off after blink duration
        if (now_ms - last_blink >= LED_HEARTBEAT_BLINK_MS) {
            write_sysfs_int(LED_MAX_SCALE_F1F2, 0);
            blink_on = false;
        }
        return;
    }

    if (now_ms - last_blink >= LED_HEARTBEAT_INTERVAL_MS) {
        // Set f1 to green static blink
        write_sysfs_str(LED_EFFECT_RGB_F1, "00FF00 ");
        write_sysfs_int(LED_EFFECT_F1, 4);  // static
        write_sysfs_int(LED_EFFECT_CYCLES_F1, 1);
        write_sysfs_int(LED_MAX_SCALE_F1F2, 10);  // dim green
        last_blink = now_ms;
        blink_on = true;
    }
}

/**
 * Check if the hardware power switch is in "lock" position
 * GPIO 243: 1 = switch ON (lock), 0 = switch OFF (unlock)
 */
bool screen_switch_is_on(void) {
    int value = read_sysfs_int(GPIO_SWITCH_PATH);
    return (value == 1);
}

/**
 * Trigger system suspend (deep sleep)
 * Uses echo mem > /sys/power/state like NextUI
 */
void screen_system_suspend(void) {
    printf("[SCREEN] System suspend...\n");

    // Save brightness before suspend
    if (g_disp_fd >= 0) {
        int current = disp_get_brightness();
        if (current > 0) {
            g_saved_brightness_lcd = current;
        }
    }

    // Trigger system suspend
    FILE *f = fopen("/sys/power/state", "w");
    if (f) {
        fprintf(f, "mem");
        fclose(f);
        // System suspends here, resumes when power button pressed
    } else {
        perror("[SCREEN] Failed to open /sys/power/state");
    }

    // After resume, restore brightness
    printf("[SCREEN] Resumed from suspend\n");
    if (g_disp_fd >= 0 && g_saved_brightness_lcd > 0) {
        disp_set_brightness(g_saved_brightness_lcd);
    }
}
