/**
 * Menu System - Options menu for the player
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
 * Menu items
 */
typedef enum {
    MENU_SHUFFLE,
    MENU_REPEAT,
    MENU_SLEEP,
    MENU_THEME,
    MENU_YOUTUBE,
    MENU_EXIT,
    MENU_ITEM_COUNT
} MenuItem;

/**
 * Initialize menu system
 */
void menu_init(void);

/**
 * Move cursor up/down
 * @param direction -1 for up, 1 for down
 */
void menu_move_cursor(int direction);

/**
 * Toggle/cycle the currently selected option
 * @return true if EXIT was selected
 */
bool menu_select(void);

/**
 * Get current cursor position
 */
int menu_get_cursor(void);

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
 * Check if YouTube was selected
 * @return true if MENU_YOUTUBE was last selected
 */
bool menu_youtube_selected(void);

/**
 * Reset YouTube selection flag
 */
void menu_reset_youtube(void);

#endif // MENU_H
