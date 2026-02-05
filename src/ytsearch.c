/**
 * YouTube Search UI State Implementation
 *
 * Manages search input, results list, and download progress states.
 * Follows patterns from filemenu.c (character picker) and browser.c (list).
 */

#include "ytsearch.h"
#include "youtube.h"
#include <stdio.h>
#include <string.h>

// Character set for search input - QWERTY layout grid
#define KBD_COLS 10
#define KBD_ROWS 5
static const char CHARSET[KBD_ROWS][KBD_COLS + 1] = {
    "1234567890",  // Row 0: numbers
    "QWERTYUIOP",  // Row 1: QWERTY top row
    "ASDFGHJKL ",  // Row 2: QWERTY middle row (space at end)
    "ZXCVBNM-._",  // Row 3: QWERTY bottom row + symbols
    " ()[]{}   ",  // Row 4: space and brackets
};

// Visible items in results list
#define VISIBLE_RESULTS 7

// Current state
static YTSearchState g_state = YTSEARCH_INPUT;

// Input state
static char g_query[128] = {0};
static int g_query_cursor = 0;  // Position in text
static int g_kbd_row = 0;       // Keyboard grid row
static int g_kbd_col = 0;       // Keyboard grid column

// Search results
static YouTubeResult g_results[YOUTUBE_MAX_RESULTS];
static int g_result_count = 0;
static int g_results_cursor = 0;
static int g_scroll_offset = 0;

// Download state
static int g_download_progress = 0;
static char g_download_status[128] = {0};
static char g_download_title[256] = {0};
static bool g_download_cancelled = false;
static const char *g_downloaded_path = NULL;

// Error state
static char g_error[256] = {0};

// Search pending flag (for async-like behavior)
static bool g_search_pending = false;

// Render callback for progress updates
static YTSearchRenderCallback g_render_callback = NULL;

void ytsearch_init(void) {
    g_state = YTSEARCH_INPUT;
    g_query[0] = '\0';
    g_query_cursor = 0;
    g_kbd_row = 0;
    g_kbd_col = 0;
    g_result_count = 0;
    g_results_cursor = 0;
    g_scroll_offset = 0;
    g_download_progress = 0;
    g_download_status[0] = '\0';
    g_download_title[0] = '\0';
    g_download_cancelled = false;
    g_downloaded_path = NULL;
    g_error[0] = '\0';
    g_search_pending = false;
}

YTSearchState ytsearch_get_state(void) {
    return g_state;
}

void ytsearch_set_state(YTSearchState state) {
    g_state = state;
}

// ============================================================================
// Input State
// ============================================================================

const char* ytsearch_get_query(void) {
    return g_query;
}

int ytsearch_get_cursor(void) {
    return g_query_cursor;
}

void ytsearch_move_kbd(int dx, int dy) {
    g_kbd_col += dx;
    g_kbd_row += dy;

    // Wrap columns
    if (g_kbd_col < 0) g_kbd_col = KBD_COLS - 1;
    if (g_kbd_col >= KBD_COLS) g_kbd_col = 0;

    // Wrap rows
    if (g_kbd_row < 0) g_kbd_row = KBD_ROWS - 1;
    if (g_kbd_row >= KBD_ROWS) g_kbd_row = 0;
}

void ytsearch_move_pos(int delta) {
    g_query_cursor += delta;
    if (g_query_cursor < 0) g_query_cursor = 0;
    int len = strlen(g_query);
    if (g_query_cursor > len) g_query_cursor = len;
}

void ytsearch_insert(void) {
    int len = strlen(g_query);
    if (len >= (int)sizeof(g_query) - 1) return;

    char c = CHARSET[g_kbd_row][g_kbd_col];
    if (c == '\0') return;  // Invalid position

    // Shift characters to make room
    for (int i = len; i >= g_query_cursor; i--) {
        g_query[i + 1] = g_query[i];
    }
    g_query[g_query_cursor] = c;
    g_query_cursor++;
}

void ytsearch_delete(void) {
    int len = strlen(g_query);
    if (g_query_cursor == 0 || len == 0) return;

    // Shift characters to fill gap
    for (int i = g_query_cursor - 1; i < len; i++) {
        g_query[i] = g_query[i + 1];
    }
    g_query_cursor--;
}

char ytsearch_get_selected_char(void) {
    char c = CHARSET[g_kbd_row][g_kbd_col];
    if (c == '\0') return ' ';  // Empty position
    return c;
}

void ytsearch_get_kbd_pos(int *row, int *col) {
    if (row) *row = g_kbd_row;
    if (col) *col = g_kbd_col;
}

void ytsearch_get_kbd_size(int *cols, int *rows) {
    if (cols) *cols = KBD_COLS;
    if (rows) *rows = KBD_ROWS;
}

char ytsearch_get_char_at(int row, int col) {
    if (row < 0 || row >= KBD_ROWS || col < 0 || col >= KBD_COLS) {
        return '\0';
    }
    return CHARSET[row][col];
}

bool ytsearch_has_query(void) {
    // Need at least 2 characters for a meaningful search
    return strlen(g_query) >= 2;
}

// ============================================================================
// Search Execution
// ============================================================================

bool ytsearch_execute_search(void) {
    if (!ytsearch_has_query()) {
        snprintf(g_error, sizeof(g_error), "Enter at least 2 characters");
        return false;
    }

    if (!youtube_is_available()) {
        snprintf(g_error, sizeof(g_error), "YouTube unavailable");
        return false;
    }

    g_error[0] = '\0';
    g_state = YTSEARCH_SEARCHING;
    g_search_pending = true;

    printf("[YTSEARCH] Starting search for: %s\n", g_query);
    return true;
}

bool ytsearch_update_search(void) {
    if (g_state != YTSEARCH_SEARCHING || !g_search_pending) {
        return false;
    }

    // Perform the actual search (blocking)
    g_search_pending = false;
    g_result_count = youtube_search(g_query, g_results, YOUTUBE_MAX_RESULTS);

    if (g_result_count < 0) {
        // Error
        const char *err = youtube_get_error();
        snprintf(g_error, sizeof(g_error), "%s", err ? err : "Search failed");
        g_result_count = 0;
        g_state = YTSEARCH_INPUT;
        return true;
    }

    if (g_result_count == 0) {
        snprintf(g_error, sizeof(g_error), "No results for '%s'", g_query);
        g_state = YTSEARCH_INPUT;
        return true;
    }

    // Success - switch to results
    g_results_cursor = 0;
    g_scroll_offset = 0;
    g_state = YTSEARCH_RESULTS;

    printf("[YTSEARCH] Search complete: %d results\n", g_result_count);
    return true;
}

// ============================================================================
// Results State
// ============================================================================

int ytsearch_get_result_count(void) {
    return g_result_count;
}

const YouTubeResult* ytsearch_get_result(int index) {
    if (index < 0 || index >= g_result_count) {
        return NULL;
    }
    return &g_results[index];
}

int ytsearch_get_results_cursor(void) {
    return g_results_cursor;
}

void ytsearch_move_results_cursor(int delta) {
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

int ytsearch_get_scroll_offset(void) {
    return g_scroll_offset;
}

// ============================================================================
// Download State
// ============================================================================

// Download progress callback
static bool download_progress_callback(int percent, const char *status) {
    g_download_progress = percent;
    if (status) {
        strncpy(g_download_status, status, sizeof(g_download_status) - 1);
    }

    // Call render callback to update UI during blocking download
    if (g_render_callback) {
        g_render_callback();
    }

    return !g_download_cancelled;
}

void ytsearch_set_render_callback(YTSearchRenderCallback callback) {
    g_render_callback = callback;
}

bool ytsearch_start_download(void) {
    if (g_result_count == 0 || g_results_cursor < 0 || g_results_cursor >= g_result_count) {
        return false;
    }

    const YouTubeResult *result = &g_results[g_results_cursor];

    g_download_progress = 0;
    g_download_cancelled = false;
    g_downloaded_path = NULL;
    strncpy(g_download_title, result->title, sizeof(g_download_title) - 1);
    snprintf(g_download_status, sizeof(g_download_status), "Starting download...");

    g_state = YTSEARCH_DOWNLOADING;

    printf("[YTSEARCH] Starting download: %s - %s\n", result->id, result->title);
    return true;
}

const char* ytsearch_update_download(void) {
    if (g_state != YTSEARCH_DOWNLOADING) {
        return NULL;
    }

    if (g_downloaded_path) {
        // Already downloaded
        return g_downloaded_path;
    }

    // Perform download (blocking with progress callback)
    const YouTubeResult *result = &g_results[g_results_cursor];
    g_downloaded_path = youtube_download(result->id, download_progress_callback);

    if (!g_downloaded_path) {
        const char *err = youtube_get_error();
        snprintf(g_error, sizeof(g_error), "%s", err ? err : "Download failed");
        g_state = YTSEARCH_RESULTS;
        return NULL;
    }

    return g_downloaded_path;
}

int ytsearch_get_download_progress(void) {
    return g_download_progress;
}

const char* ytsearch_get_download_status(void) {
    return g_download_status[0] ? g_download_status : NULL;
}

const char* ytsearch_get_download_title(void) {
    return g_download_title[0] ? g_download_title : NULL;
}

void ytsearch_cancel_download(void) {
    g_download_cancelled = true;
}

// ============================================================================
// Error Handling
// ============================================================================

const char* ytsearch_get_error(void) {
    return g_error[0] ? g_error : NULL;
}

void ytsearch_clear_error(void) {
    g_error[0] = '\0';
}
