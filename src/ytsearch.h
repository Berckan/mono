/**
 * YouTube Search UI State Management
 *
 * Manages the search input UI and results list state.
 * Uses character picker pattern from filemenu.c and list pattern from browser.c.
 */

#ifndef YTSEARCH_H
#define YTSEARCH_H

#include <stdbool.h>
#include "youtube.h"

/**
 * YouTube search UI states
 */
typedef enum {
    YTSEARCH_INPUT,       // Character picker for search query
    YTSEARCH_SEARCHING,   // Loading/searching
    YTSEARCH_RESULTS,     // Results list
    YTSEARCH_DOWNLOADING  // Download progress
} YTSearchState;

/**
 * Initialize YouTube search UI
 * Resets to input state with empty query
 */
void ytsearch_init(void);

/**
 * Get current UI state
 */
YTSearchState ytsearch_get_state(void);

/**
 * Set UI state (for async operation completion)
 */
void ytsearch_set_state(YTSearchState state);

// ============================================================================
// Input State (Character Picker)
// ============================================================================

/**
 * Get current search query text
 */
const char* ytsearch_get_query(void);

/**
 * Get cursor position in query text
 */
int ytsearch_get_cursor(void);

/**
 * Move keyboard cursor in grid
 * @param dx -1 for left, 1 for right
 * @param dy -1 for up, 1 for down
 */
void ytsearch_move_kbd(int dx, int dy);

/**
 * Move cursor position in text (left/right)
 * @param delta -1 for left, 1 for right
 */
void ytsearch_move_pos(int delta);

/**
 * Insert currently selected character at cursor
 */
void ytsearch_insert(void);

/**
 * Delete character before cursor
 */
void ytsearch_delete(void);

/**
 * Get currently selected character for insertion
 */
char ytsearch_get_selected_char(void);

/**
 * Get keyboard grid position
 */
void ytsearch_get_kbd_pos(int *row, int *col);

/**
 * Get keyboard grid dimensions
 */
void ytsearch_get_kbd_size(int *cols, int *rows);

/**
 * Get character at keyboard grid position
 */
char ytsearch_get_char_at(int row, int col);

/**
 * Check if query is valid for search
 */
bool ytsearch_has_query(void);

// ============================================================================
// Search Execution
// ============================================================================

/**
 * Execute YouTube search with current query
 * Changes state to YTSEARCH_SEARCHING, then YTSEARCH_RESULTS
 * @return true if search started
 */
bool ytsearch_execute_search(void);

/**
 * Update search (call each frame when YTSEARCH_SEARCHING)
 * Performs the actual search operation
 * @return true when search is complete (state changed to RESULTS)
 */
bool ytsearch_update_search(void);

// ============================================================================
// Results State
// ============================================================================

/**
 * Get number of search results
 */
int ytsearch_get_result_count(void);

/**
 * Get a search result by index
 * @return Pointer to result, or NULL if invalid index
 */
const YouTubeResult* ytsearch_get_result(int index);

/**
 * Get current cursor position in results list
 */
int ytsearch_get_results_cursor(void);

/**
 * Move cursor in results list
 * @param delta -1 for up, 1 for down
 */
void ytsearch_move_results_cursor(int delta);

/**
 * Get scroll offset for results list
 */
int ytsearch_get_scroll_offset(void);

// ============================================================================
// Download State
// ============================================================================

/**
 * Start download of currently selected result
 * Changes state to YTSEARCH_DOWNLOADING
 * @return true if download started
 */
bool ytsearch_start_download(void);

/**
 * Update download progress (call each frame when YTSEARCH_DOWNLOADING)
 * @return Path to downloaded file when complete, NULL while downloading
 */
const char* ytsearch_update_download(void);

/**
 * Get download progress (0-100)
 */
int ytsearch_get_download_progress(void);

/**
 * Get download status message
 */
const char* ytsearch_get_download_status(void);

/**
 * Get title of currently downloading track
 */
const char* ytsearch_get_download_title(void);

/**
 * Cancel current download
 */
void ytsearch_cancel_download(void);

// ============================================================================
// Error Handling
// ============================================================================

/**
 * Get last error message
 * @return Error message or NULL
 */
const char* ytsearch_get_error(void);

/**
 * Clear error state
 */
void ytsearch_clear_error(void);

#endif // YTSEARCH_H
