/**
 * File Menu - Context menu for file operations (delete, rename)
 */

#ifndef FILEMENU_H
#define FILEMENU_H

#include <stdbool.h>

/**
 * File menu options
 */
typedef enum {
    FILEMENU_RENAME,
    FILEMENU_DELETE,
    FILEMENU_SCAN_METADATA,     // Only shown for directories
    FILEMENU_RESTORE_METADATA,  // Only shown for directories (if backup exists)
    FILEMENU_CANCEL,
    FILEMENU_COUNT
} FileMenuOption;

/**
 * File menu action results
 */
typedef enum {
    FILEMENU_RESULT_NONE,
    FILEMENU_RESULT_DELETED,
    FILEMENU_RESULT_RENAMED,
    FILEMENU_RESULT_SCAN_STARTED,
    FILEMENU_RESULT_RESTORED,
    FILEMENU_RESULT_CANCELLED
} FileMenuResult;

/**
 * Initialize file menu with target path
 * @param path Path to file or directory
 * @param is_directory True if target is a directory
 */
void filemenu_init(const char *path, bool is_directory);

/**
 * Get current cursor position
 */
int filemenu_get_cursor(void);

/**
 * Get actual menu option (adjusts for hidden options)
 */
int filemenu_get_actual_option(void);

/**
 * Move cursor up/down
 */
void filemenu_move_cursor(int delta);

/**
 * Select current option
 * @return true if action should close menu
 */
bool filemenu_select(void);

/**
 * Check if waiting for confirmation
 */
bool filemenu_needs_confirm(void);

/**
 * Confirm delete action
 * @param confirmed True to proceed with delete
 * @return Result of the action
 */
FileMenuResult filemenu_confirm_delete(bool confirmed);

/**
 * Get target filename (for display)
 */
const char* filemenu_get_filename(void);

/**
 * Get target path
 */
const char* filemenu_get_path(void);

/**
 * Check if target is a directory
 */
bool filemenu_is_directory(void);

// Rename functionality
/**
 * Initialize rename mode
 */
void filemenu_rename_init(void);

/**
 * Get current rename buffer
 */
const char* filemenu_rename_get_text(void);

/**
 * Get cursor position in rename buffer
 */
int filemenu_rename_get_cursor(void);

/**
 * Move keyboard cursor in grid (up/down/left/right)
 */
void filemenu_rename_move_kbd(int dx, int dy);

/**
 * Move cursor position in text
 */
void filemenu_rename_move_pos(int delta);

/**
 * Insert current character at cursor
 */
void filemenu_rename_insert(void);

/**
 * Delete character at cursor
 */
void filemenu_rename_delete(void);

/**
 * Get currently selected character
 */
char filemenu_rename_get_selected_char(void);

/**
 * Get keyboard grid position (for rendering)
 */
void filemenu_rename_get_kbd_pos(int *row, int *col);

/**
 * Get keyboard grid dimensions
 */
void filemenu_rename_get_kbd_size(int *cols, int *rows);

/**
 * Get character at keyboard grid position
 */
char filemenu_rename_get_char_at(int row, int col);

/**
 * Confirm rename
 * @return Result of the action
 */
FileMenuResult filemenu_rename_confirm(void);

#endif // FILEMENU_H
