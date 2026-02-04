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

#endif // SCREEN_H
