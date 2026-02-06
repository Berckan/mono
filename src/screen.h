/**
 * Screen Brightness Control
 *
 * Provides dimming functionality for battery saving during playback.
 * Uses /sys/class/backlight interface on Trimui Brick.
 */

#ifndef SCREEN_H
#define SCREEN_H

#include <stdbool.h>

/**
 * Initialize screen control module
 * Saves current brightness for later restoration
 * @return 0 on success, -1 on failure
 */
int screen_init(void);

/**
 * Dim screen to minimum (10%)
 * Call screen_init() first to save original brightness
 */
void screen_dim(void);

/**
 * Restore screen to saved brightness
 */
void screen_restore(void);

/**
 * Toggle between dimmed and normal brightness
 * @return true if now dimmed, false if restored
 */
bool screen_toggle_dim(void);

/**
 * Check if screen is currently dimmed
 * @return true if dimmed
 */
bool screen_is_dimmed(void);

/**
 * Cleanup screen control module
 * Restores brightness if dimmed
 */
void screen_cleanup(void);

/**
 * Turn display completely off (via framebuffer blank)
 * More power-saving than dimming - turns off entire panel
 */
void screen_off(void);

/**
 * Turn display back on (via framebuffer unblank)
 * Restores previous brightness level
 */
void screen_on(void);

/**
 * Check if display is currently off
 * @return true if display is blanked
 */
bool screen_is_off(void);

/**
 * Check if hardware power switch is in "lock" position
 * @return true if switch is ON (device should be locked)
 */
bool screen_switch_is_on(void);

/**
 * LED heartbeat for pocket mode: brief green blink on f1 every 10 seconds.
 * Call from main loop with SDL_GetTicks() value.
 */
void screen_update_led_heartbeat(unsigned int now_ms);

/**
 * Trigger system suspend (deep sleep)
 * Uses echo mem > /sys/power/state like NextUI
 * System will wake on power button press
 */
void screen_system_suspend(void);

#endif // SCREEN_H
