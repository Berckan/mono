/**
 * Favorites System Implementation
 *
 * Stores favorites as a JSON array of file paths.
 * Uses the same data directory as state persistence.
 *
 * File format:
 * {
 *   "favorites": [
 *     "/path/to/file1.mp3",
 *     "/path/to/file2.mp3"
 *   ]
 * }
 */

#include "favorites.h"
#include "state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAVORITES_FILENAME "favorites.json"

// Favorites storage
static char g_favorites[MAX_FAVORITES][512];
static int g_favorites_count = 0;
static char g_favorites_path[512] = {0};
static bool g_dirty = false;  // Track if changes need saving

// Favorites playback mode
static bool g_favorites_playback_mode = false;
static int g_favorites_playback_index = 0;

/**
 * Build favorites file path
 */
static void build_path(void) {
    const char *data_dir = state_get_data_dir();
    if (data_dir && data_dir[0]) {
        snprintf(g_favorites_path, sizeof(g_favorites_path), "%s/%s", data_dir, FAVORITES_FILENAME);
    }
}

/**
 * Load favorites from disk
 */
static bool load_favorites(void) {
    if (g_favorites_path[0] == '\0') {
        build_path();
    }

    FILE *f = fopen(g_favorites_path, "r");
    if (!f) {
        printf("[FAV] No favorites file found\n");
        return false;
    }

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 262144) {  // 256KB max
        fclose(f);
        return false;
    }

    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        return false;
    }

    size_t read_size = fread(json, 1, size, f);
    fclose(f);

    if (read_size != (size_t)size) {
        free(json);
        return false;
    }
    json[size] = '\0';

    // Simple parsing: find each quoted string in the "favorites" array
    const char *arr_start = strstr(json, "\"favorites\"");
    if (!arr_start) {
        free(json);
        return false;
    }

    arr_start = strchr(arr_start, '[');
    if (!arr_start) {
        free(json);
        return false;
    }

    const char *arr_end = strchr(arr_start, ']');
    if (!arr_end) {
        free(json);
        return false;
    }

    // Parse each path in the array
    g_favorites_count = 0;
    const char *p = arr_start + 1;

    while (p < arr_end && g_favorites_count < MAX_FAVORITES) {
        // Find opening quote
        const char *start = strchr(p, '"');
        if (!start || start >= arr_end) break;
        start++;

        // Find closing quote (handle escaped quotes)
        const char *end = start;
        while (end < arr_end) {
            end = strchr(end, '"');
            if (!end) break;
            // Check if escaped
            if (end > start && *(end - 1) == '\\') {
                end++;
                continue;
            }
            break;
        }

        if (!end || end >= arr_end) break;

        // Copy path
        size_t len = end - start;
        if (len > 0 && len < sizeof(g_favorites[0])) {
            strncpy(g_favorites[g_favorites_count], start, len);
            g_favorites[g_favorites_count][len] = '\0';
            g_favorites_count++;
        }

        p = end + 1;
    }

    free(json);
    printf("[FAV] Loaded %d favorites\n", g_favorites_count);
    return true;
}

int favorites_init(void) {
    g_favorites_count = 0;
    g_dirty = false;

    build_path();
    load_favorites();

    return 0;
}

void favorites_cleanup(void) {
    if (g_dirty) {
        favorites_save();
    }
    g_favorites_count = 0;
}

bool favorites_add(const char *path) {
    if (!path || path[0] == '\0') return false;

    // Check if already exists
    if (favorites_is_favorite(path)) {
        return false;
    }

    // Check capacity
    if (g_favorites_count >= MAX_FAVORITES) {
        fprintf(stderr, "[FAV] Favorites list is full\n");
        return false;
    }

    // Add to list
    strncpy(g_favorites[g_favorites_count], path, sizeof(g_favorites[0]) - 1);
    g_favorites[g_favorites_count][sizeof(g_favorites[0]) - 1] = '\0';
    g_favorites_count++;
    g_dirty = true;
    favorites_save();  // Persist immediately - NextUI/MinUI kills app without cleanup

    printf("[FAV] Added: %s\n", path);
    return true;
}

bool favorites_remove(const char *path) {
    if (!path || path[0] == '\0') return false;

    for (int i = 0; i < g_favorites_count; i++) {
        if (strcmp(g_favorites[i], path) == 0) {
            // Shift remaining entries
            for (int j = i; j < g_favorites_count - 1; j++) {
                strcpy(g_favorites[j], g_favorites[j + 1]);
            }
            g_favorites_count--;
            g_dirty = true;
            favorites_save();  // Persist immediately - NextUI/MinUI kills app without cleanup
            printf("[FAV] Removed: %s\n", path);
            return true;
        }
    }

    return false;
}

bool favorites_toggle(const char *path) {
    if (favorites_is_favorite(path)) {
        favorites_remove(path);
        return false;  // No longer a favorite
    } else {
        favorites_add(path);
        return true;  // Now a favorite
    }
}

bool favorites_is_favorite(const char *path) {
    if (!path || path[0] == '\0') return false;

    for (int i = 0; i < g_favorites_count; i++) {
        if (strcmp(g_favorites[i], path) == 0) {
            return true;
        }
    }
    return false;
}

int favorites_get_count(void) {
    return g_favorites_count;
}

const char* favorites_get_path(int index) {
    if (index < 0 || index >= g_favorites_count) {
        return NULL;
    }
    return g_favorites[index];
}

bool favorites_save(void) {
    if (g_favorites_path[0] == '\0') {
        build_path();
    }

    FILE *f = fopen(g_favorites_path, "w");
    if (!f) {
        fprintf(stderr, "[FAV] Failed to open %s for writing\n", g_favorites_path);
        return false;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"favorites\": [\n");

    for (int i = 0; i < g_favorites_count; i++) {
        // Escape special characters in path
        fprintf(f, "    \"");

        for (const char *p = g_favorites[i]; *p; p++) {
            if (*p == '"' || *p == '\\') {
                fputc('\\', f);
            }
            fputc(*p, f);
        }

        fprintf(f, "\"");
        if (i < g_favorites_count - 1) {
            fprintf(f, ",");
        }
        fprintf(f, "\n");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    g_dirty = false;
    printf("[FAV] Saved %d favorites to %s\n", g_favorites_count, g_favorites_path);
    return true;
}

// ========================================
// Favorites Playback Mode Implementation
// ========================================

void favorites_set_playback_mode(bool enabled, int start_index) {
    g_favorites_playback_mode = enabled;
    if (enabled && start_index >= 0 && start_index < g_favorites_count) {
        g_favorites_playback_index = start_index;
        printf("[FAV] Playback mode enabled, starting at index %d\n", start_index);
    } else if (!enabled) {
        g_favorites_playback_index = 0;
        printf("[FAV] Playback mode disabled\n");
    }
}

bool favorites_is_playback_mode(void) {
    return g_favorites_playback_mode;
}

int favorites_advance_playback(int delta) {
    if (!g_favorites_playback_mode || g_favorites_count == 0) {
        return -1;
    }

    int new_index = g_favorites_playback_index + delta;

    // Wrap around
    if (new_index < 0) {
        new_index = g_favorites_count - 1;
    } else if (new_index >= g_favorites_count) {
        new_index = 0;
    }

    g_favorites_playback_index = new_index;
    printf("[FAV] Advanced to index %d: %s\n", new_index, g_favorites[new_index]);
    return new_index;
}

const char* favorites_get_current_playback_path(void) {
    if (!g_favorites_playback_mode || g_favorites_count == 0) {
        return NULL;
    }
    if (g_favorites_playback_index < 0 || g_favorites_playback_index >= g_favorites_count) {
        return NULL;
    }
    return g_favorites[g_favorites_playback_index];
}

int favorites_get_playback_index(void) {
    return g_favorites_playback_index;
}

void favorites_set_playback_index(int index) {
    if (index >= 0 && index < g_favorites_count) {
        g_favorites_playback_index = index;
        printf("[FAV] Set playback index to %d\n", index);
    }
}
