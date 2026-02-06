/**
 * Spotify Search UI State Implementation
 *
 * Manages search input, results list state for Spotify.
 * Mirrors ytsearch.c 1:1 but uses Spotify Web API for search.
 */

#include "spsearch.h"
#include "spotify.h"
#include <stdio.h>
#include <string.h>

// Character set for search input - QWERTY layout grid (same as ytsearch.c)
#define KBD_COLS 10
#define KBD_ROWS 5
static const char CHARSET[KBD_ROWS][KBD_COLS + 1] = {
    "1234567890",   // Row 0: numbers
    "QWERTYUIOP",   // Row 1: QWERTY top row
    "ASDFGHJKL ",   // Row 2: QWERTY middle row (space at end)
    "ZXCVBNM-._",   // Row 3: QWERTY bottom row + symbols
    " ()[]{}   ",   // Row 4: space and brackets
};

// Visible items in results list
#define VISIBLE_RESULTS 7

// Current state
static SpSearchState g_state = SPSEARCH_INPUT;

// Input state
static char g_query[128] = {0};
static int g_query_cursor = 0;
static int g_kbd_row = 0;
static int g_kbd_col = 0;

// Search results
static SpotifyTrack g_results[SPOTIFY_MAX_RESULTS];
static int g_result_count = 0;
static int g_results_cursor = 0;
static int g_scroll_offset = 0;

// Error state
static char g_error[256] = {0};

// Search pending flag
static bool g_search_pending = false;

void spsearch_init(void) {
    g_state = SPSEARCH_INPUT;
    g_query[0] = '\0';
    g_query_cursor = 0;
    g_kbd_row = 0;
    g_kbd_col = 0;
    g_result_count = 0;
    g_results_cursor = 0;
    g_scroll_offset = 0;
    g_error[0] = '\0';
    g_search_pending = false;
}

SpSearchState spsearch_get_state(void) {
    return g_state;
}

void spsearch_set_state(SpSearchState state) {
    g_state = state;
}

// ============================================================================
// Input State
// ============================================================================

const char* spsearch_get_query(void) {
    return g_query;
}

int spsearch_get_cursor(void) {
    return g_query_cursor;
}

void spsearch_move_kbd(int dx, int dy) {
    g_kbd_col += dx;
    g_kbd_row += dy;

    // Wrap columns
    if (g_kbd_col < 0) g_kbd_col = KBD_COLS - 1;
    if (g_kbd_col >= KBD_COLS) g_kbd_col = 0;

    // Wrap rows
    if (g_kbd_row < 0) g_kbd_row = KBD_ROWS - 1;
    if (g_kbd_row >= KBD_ROWS) g_kbd_row = 0;
}

void spsearch_insert(void) {
    int len = strlen(g_query);
    if (len >= (int)sizeof(g_query) - 1) return;

    char c = CHARSET[g_kbd_row][g_kbd_col];
    if (c == '\0') return;

    // Shift characters to make room
    for (int i = len; i >= g_query_cursor; i--) {
        g_query[i + 1] = g_query[i];
    }
    g_query[g_query_cursor] = c;
    g_query_cursor++;
}

void spsearch_delete(void) {
    int len = strlen(g_query);
    if (g_query_cursor == 0 || len == 0) return;

    // Shift characters to fill gap
    for (int i = g_query_cursor - 1; i < len; i++) {
        g_query[i] = g_query[i + 1];
    }
    g_query_cursor--;
}

char spsearch_get_selected_char(void) {
    char c = CHARSET[g_kbd_row][g_kbd_col];
    if (c == '\0') return ' ';
    return c;
}

void spsearch_get_kbd_pos(int *row, int *col) {
    if (row) *row = g_kbd_row;
    if (col) *col = g_kbd_col;
}

void spsearch_get_kbd_size(int *cols, int *rows) {
    if (cols) *cols = KBD_COLS;
    if (rows) *rows = KBD_ROWS;
}

char spsearch_get_char_at(int row, int col) {
    if (row < 0 || row >= KBD_ROWS || col < 0 || col >= KBD_COLS) {
        return '\0';
    }
    return CHARSET[row][col];
}

bool spsearch_has_query(void) {
    return strlen(g_query) >= 2;
}

// ============================================================================
// Search Execution
// ============================================================================

bool spsearch_execute_search(void) {
    if (!spsearch_has_query()) {
        snprintf(g_error, sizeof(g_error), "Enter at least 2 characters");
        return false;
    }

    if (!spotify_is_available()) {
        snprintf(g_error, sizeof(g_error), "Spotify unavailable");
        return false;
    }

    g_error[0] = '\0';
    g_state = SPSEARCH_SEARCHING;
    g_search_pending = true;

    printf("[SPSEARCH] Starting search for: %s\n", g_query);
    return true;
}

bool spsearch_update_search(void) {
    if (g_state != SPSEARCH_SEARCHING || !g_search_pending) {
        return false;
    }

    // Perform the actual search (blocking - same pattern as ytsearch.c)
    g_search_pending = false;
    g_result_count = spotify_search(g_query, g_results, SPOTIFY_MAX_RESULTS);

    if (g_result_count < 0) {
        const char *err = spotify_get_error();
        snprintf(g_error, sizeof(g_error), "%s", err ? err : "Search failed");
        g_result_count = 0;
        g_state = SPSEARCH_INPUT;
        return true;
    }

    if (g_result_count == 0) {
        snprintf(g_error, sizeof(g_error), "No results for '%s'", g_query);
        g_state = SPSEARCH_INPUT;
        return true;
    }

    // Success - switch to results
    g_results_cursor = 0;
    g_scroll_offset = 0;
    g_state = SPSEARCH_RESULTS;

    printf("[SPSEARCH] Search complete: %d results\n", g_result_count);
    return true;
}

// ============================================================================
// Results State
// ============================================================================

int spsearch_get_result_count(void) {
    return g_result_count;
}

const SpotifyTrack* spsearch_get_result(int index) {
    if (index < 0 || index >= g_result_count) {
        return NULL;
    }
    return &g_results[index];
}

int spsearch_get_results_cursor(void) {
    return g_results_cursor;
}

void spsearch_move_results_cursor(int delta) {
    g_results_cursor += delta;

    if (g_results_cursor < 0) {
        g_results_cursor = g_result_count - 1;
    }
    if (g_results_cursor >= g_result_count) {
        g_results_cursor = 0;
    }

    // Adjust scroll offset to keep cursor visible
    if (g_results_cursor < g_scroll_offset) {
        g_scroll_offset = g_results_cursor;
    }
    if (g_results_cursor >= g_scroll_offset + VISIBLE_RESULTS) {
        g_scroll_offset = g_results_cursor - VISIBLE_RESULTS + 1;
    }
}

int spsearch_get_scroll_offset(void) {
    return g_scroll_offset;
}

// ============================================================================
// Error Handling
// ============================================================================

const char* spsearch_get_error(void) {
    return g_error[0] ? g_error : NULL;
}

void spsearch_clear_error(void) {
    g_error[0] = '\0';
}
