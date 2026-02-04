/**
 * File Browser - Directory navigation for music files
 */

#ifndef BROWSER_H
#define BROWSER_H

#include <stdbool.h>

/**
 * File entry types
 */
typedef enum {
    ENTRY_FILE,
    ENTRY_DIRECTORY
} EntryType;

/**
 * File entry in browser
 */
typedef struct {
    char name[256];
    char full_path[512];
    EntryType type;
} FileEntry;

/**
 * Initialize file browser at given path
 * @param base_path Starting directory
 * @return 0 on success, -1 on failure
 */
int browser_init(const char *base_path);

/**
 * Cleanup browser resources
 */
void browser_cleanup(void);

/**
 * Move cursor up/down in file list
 * @param delta -1 for up, 1 for down
 * @return true if cursor moved
 */
bool browser_move_cursor(int delta);

/**
 * Select current item (enter directory or select file)
 * @return true if a file was selected (not a directory)
 */
bool browser_select_current(void);

/**
 * Go up one directory
 * @return true if moved up, false if at root
 */
bool browser_go_up(void);

/**
 * Get current cursor position
 * @return Current index
 */
int browser_get_cursor(void);

/**
 * Get number of entries in current directory
 * @return Number of entries
 */
int browser_get_count(void);

/**
 * Get entry at index
 * @param index Entry index
 * @return Pointer to entry, NULL if invalid
 */
const FileEntry* browser_get_entry(int index);

/**
 * Get currently selected file path
 * @return Path to selected file, NULL if none
 */
const char* browser_get_selected_path(void);

/**
 * Get current directory path
 * @return Current directory path
 */
const char* browser_get_current_path(void);

/**
 * Get scroll offset for list display
 * @return Scroll offset
 */
int browser_get_scroll_offset(void);

/**
 * Set cursor position directly (for state restoration)
 * @param pos New cursor position
 */
void browser_set_cursor(int pos);

/**
 * Navigate to a specific directory
 * @param path Directory path to navigate to
 * @return 0 on success, -1 on failure
 */
int browser_navigate_to(const char *path);

#endif // BROWSER_H
