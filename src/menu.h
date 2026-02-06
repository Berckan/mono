/**
 * Menu System - Context-sensitive options menu
 *
 * Player menu:  Shuffle, Repeat, Sleep, Equalizer
 * Browser menu: Theme, Power
 */

#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

/**
 * Repeat modes
 */
typedef enum {
    REPEAT_OFF,
    REPEAT_ONE,
    REPEAT_ALL
} RepeatMode;

/**
 * Power modes for battery/performance trade-off
 */
typedef enum {
    POWER_MODE_BATTERY,     // 20fps, minimal processing - best battery
    POWER_MODE_BALANCED,    // 30fps (default) - good balance
    POWER_MODE_PERFORMANCE  // 60fps, smoother UI - worst battery
} PowerMode;

/**
 * Menu mode - determines which items are shown
 */
typedef enum {
    MENU_MODE_PLAYER,   // Opened from player: Shuffle, Repeat, Sleep, Equalizer
    MENU_MODE_BROWSER   // Opened from browser/home: Theme, Power
} MenuMode;

/**
 * Result of selecting a menu item
 */
typedef enum {
    MENU_RESULT_NONE,       // Stay in menu (toggled an option)
    MENU_RESULT_CLOSE,      // Close menu, return to caller
    MENU_RESULT_EQUALIZER   // Open equalizer screen
} MenuResult;

/**
 * Menu items (all possible items across modes)
 */
typedef enum {
    MENU_SHUFFLE,
    MENU_REPEAT,
    MENU_SLEEP,
    MENU_EQUALIZER,
    MENU_THEME,
    MENU_POWER,
    MENU_ITEM_COUNT
} MenuItem;

/**
 * Initialize menu system
 */
void menu_init(void);

/**
 * Open menu in specified mode (resets cursor to 0)
 * @param mode MENU_MODE_PLAYER or MENU_MODE_BROWSER
 */
void menu_open(MenuMode mode);

/**
 * Move cursor up/down within active items
 * @param direction -1 for up, 1 for down
 */
void menu_move_cursor(int direction);

/**
 * Toggle/cycle the currently selected option
 * @return MenuResult indicating what action to take
 */
MenuResult menu_select(void);

/**
 * Get current cursor position (index into active items)
 */
int menu_get_cursor(void);

/**
 * Get number of items in current mode
 */
int menu_get_item_count(void);

/**
 * Get display label for item at index in current mode
 * @param index Index into active items (0-based)
 * @return Static string with label (e.g. "Shuffle: On")
 */
const char* menu_get_item_label(int index);

/**
 * Get the MenuItem enum for the item at current cursor
 */
MenuItem menu_get_current_item(void);

/**
 * Get shuffle state
 */
bool menu_is_shuffle_enabled(void);

/**
 * Get repeat mode
 */
RepeatMode menu_get_repeat_mode(void);

/**
 * Get sleep timer remaining minutes (0 = disabled)
 */
int menu_get_sleep_remaining(void);

/**
 * Update sleep timer (call every frame)
 * @return true if timer expired and playback should stop
 */
bool menu_update_sleep_timer(void);

/**
 * Get string representation of repeat mode
 */
const char* menu_get_repeat_string(void);

/**
 * Get string representation of sleep timer
 */
const char* menu_get_sleep_string(void);

/**
 * Set shuffle state (for state restoration)
 */
void menu_set_shuffle(bool enabled);

/**
 * Set repeat mode (for state restoration)
 */
void menu_set_repeat(RepeatMode mode);

/**
 * Get current power mode
 */
PowerMode menu_get_power_mode(void);

/**
 * Set power mode (for state restoration)
 */
void menu_set_power_mode(PowerMode mode);

/**
 * Get string representation of power mode
 */
const char* menu_get_power_string(void);

#endif // MENU_H
