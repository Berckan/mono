/**
 * Spotify Search UI State Management
 *
 * Manages the search input UI and results list state for Spotify.
 * Mirrors ytsearch.h 1:1 but uses SpotifyTrack and Spotify Web API.
 */

#ifndef SPSEARCH_H
#define SPSEARCH_H

#include <stdbool.h>
#include "spotify.h"

/**
 * Spotify search UI states
 */
typedef enum {
    SPSEARCH_INPUT,       // Character picker for search query
    SPSEARCH_SEARCHING,   // Loading/searching
    SPSEARCH_RESULTS,     // Results list
} SpSearchState;

/**
 * Initialize Spotify search UI
 * Resets to input state with empty query
 */
void spsearch_init(void);

/**
 * Get current UI state
 */
SpSearchState spsearch_get_state(void);

/**
 * Set UI state
 */
void spsearch_set_state(SpSearchState state);

// ============================================================================
// Input State (Character Picker)
// ============================================================================

/**
 * Get current search query text
 */
const char* spsearch_get_query(void);

/**
 * Get cursor position in query text
 */
int spsearch_get_cursor(void);

/**
 * Move keyboard cursor in grid
 * @param dx -1 for left, 1 for right
 * @param dy -1 for up, 1 for down
 */
void spsearch_move_kbd(int dx, int dy);

/**
 * Insert currently selected character at cursor
 */
void spsearch_insert(void);

/**
 * Delete character before cursor
 */
void spsearch_delete(void);

/**
 * Get currently selected character for insertion
 */
char spsearch_get_selected_char(void);

/**
 * Get keyboard grid position
 */
void spsearch_get_kbd_pos(int *row, int *col);

/**
 * Get keyboard grid dimensions
 */
void spsearch_get_kbd_size(int *cols, int *rows);

/**
 * Get character at keyboard grid position
 */
char spsearch_get_char_at(int row, int col);

/**
 * Check if query is valid for search
 */
bool spsearch_has_query(void);

// ============================================================================
// Search Execution
// ============================================================================

/**
 * Execute Spotify search with current query
 * Changes state to SPSEARCH_SEARCHING
 * @return true if search started
 */
bool spsearch_execute_search(void);

/**
 * Update search (call each frame when SPSEARCH_SEARCHING)
 * Performs the actual search operation (blocking)
 * @return true when search is complete
 */
bool spsearch_update_search(void);

// ============================================================================
// Results State
// ============================================================================

/**
 * Get number of search results
 */
int spsearch_get_result_count(void);

/**
 * Get a search result by index
 * @return Pointer to result, or NULL if invalid index
 */
const SpotifyTrack* spsearch_get_result(int index);

/**
 * Get current cursor position in results list
 */
int spsearch_get_results_cursor(void);

/**
 * Move cursor in results list
 * @param delta -1 for up, 1 for down
 */
void spsearch_move_results_cursor(int delta);

/**
 * Get scroll offset for results list
 */
int spsearch_get_scroll_offset(void);

// ============================================================================
// Error Handling
// ============================================================================

/**
 * Get last error message
 * @return Error message or NULL
 */
const char* spsearch_get_error(void);

/**
 * Clear error state
 */
void spsearch_clear_error(void);

#endif // SPSEARCH_H
