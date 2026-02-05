/**
 * Position Persistence Implementation
 *
 * Stores playback positions in a simple JSON file.
 * Uses a hash-based approach for quick lookups.
 */

#include "positions.h"
#include "state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum number of tracked positions
#define MAX_POSITIONS 500

// Minimum position to save (don't save if < 5 seconds)
#define MIN_POSITION_SEC 5

// Position entry
typedef struct {
    char path[512];
    int position_sec;
} PositionEntry;

// Position storage
static PositionEntry g_positions[MAX_POSITIONS];
static int g_position_count = 0;
static char g_positions_path[512] = {0};
static bool g_dirty = false;  // Track if changes need saving

/**
 * Find position entry by path
 * @return Index or -1 if not found
 */
static int find_position(const char *path) {
    if (!path || !path[0]) return -1;

    for (int i = 0; i < g_position_count; i++) {
        if (strcmp(g_positions[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Simple JSON string extraction
 */
static bool extract_json_entry(const char *json, int *offset, char *path, size_t path_size, int *position) {
    const char *p = json + *offset;

    // Find opening quote for path
    const char *quote1 = strchr(p, '"');
    if (!quote1) return false;

    // Find closing quote for path
    const char *quote2 = strchr(quote1 + 1, '"');
    if (!quote2) return false;

    // Extract path
    size_t len = quote2 - quote1 - 1;
    if (len >= path_size) len = path_size - 1;
    strncpy(path, quote1 + 1, len);
    path[len] = '\0';

    // Find colon and number
    const char *colon = strchr(quote2, ':');
    if (!colon) return false;

    *position = atoi(colon + 1);

    // Find next comma or end
    const char *next = strchr(colon, ',');
    if (next) {
        *offset = (next + 1) - json;
    } else {
        *offset = strlen(json);
    }

    return true;
}

int positions_init(void) {
    // Build positions file path
    const char *data_dir = state_get_data_dir();
    if (!data_dir || !data_dir[0]) {
        fprintf(stderr, "[POSITIONS] No data directory available\n");
        return -1;
    }

    snprintf(g_positions_path, sizeof(g_positions_path), "%s/positions.json", data_dir);

    // Load existing positions
    FILE *f = fopen(g_positions_path, "r");
    if (!f) {
        printf("[POSITIONS] No saved positions found\n");
        return 0;
    }

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {  // Max 1MB
        fclose(f);
        return 0;
    }

    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        return -1;
    }

    size_t read_size = fread(json, 1, size, f);
    fclose(f);

    if (read_size != (size_t)size) {
        free(json);
        return -1;
    }
    json[size] = '\0';

    // Parse entries
    int offset = 0;
    char path[512];
    int position;

    while (g_position_count < MAX_POSITIONS) {
        if (!extract_json_entry(json, &offset, path, sizeof(path), &position)) {
            break;
        }

        if (path[0] && position > 0) {
            strncpy(g_positions[g_position_count].path, path, sizeof(g_positions[0].path) - 1);
            g_positions[g_position_count].position_sec = position;
            g_position_count++;
        }
    }

    free(json);
    printf("[POSITIONS] Loaded %d saved positions\n", g_position_count);
    return 0;
}

void positions_set(const char *path, int position_sec) {
    if (!path || !path[0]) return;

    // Don't save very short positions
    if (position_sec < MIN_POSITION_SEC) {
        positions_clear(path);
        return;
    }

    int idx = find_position(path);

    if (idx >= 0) {
        // Update existing
        if (g_positions[idx].position_sec != position_sec) {
            g_positions[idx].position_sec = position_sec;
            g_dirty = true;
        }
    } else {
        // Add new entry
        if (g_position_count < MAX_POSITIONS) {
            strncpy(g_positions[g_position_count].path, path, sizeof(g_positions[0].path) - 1);
            g_positions[g_position_count].position_sec = position_sec;
            g_position_count++;
            g_dirty = true;
        } else {
            // Storage full - remove oldest entry (index 0) and shift
            memmove(&g_positions[0], &g_positions[1], (MAX_POSITIONS - 1) * sizeof(PositionEntry));
            strncpy(g_positions[MAX_POSITIONS - 1].path, path, sizeof(g_positions[0].path) - 1);
            g_positions[MAX_POSITIONS - 1].position_sec = position_sec;
            g_dirty = true;
        }
    }
}

int positions_get(const char *path) {
    if (!path || !path[0]) return 0;

    int idx = find_position(path);
    if (idx >= 0) {
        return g_positions[idx].position_sec;
    }
    return 0;
}

void positions_clear(const char *path) {
    if (!path || !path[0]) return;

    int idx = find_position(path);
    if (idx >= 0) {
        // Remove by shifting remaining entries
        if (idx < g_position_count - 1) {
            memmove(&g_positions[idx], &g_positions[idx + 1],
                    (g_position_count - idx - 1) * sizeof(PositionEntry));
        }
        g_position_count--;
        g_dirty = true;
    }
}

void positions_save(void) {
    if (!g_dirty || g_position_count == 0) return;

    FILE *f = fopen(g_positions_path, "w");
    if (!f) {
        fprintf(stderr, "[POSITIONS] Failed to save positions\n");
        return;
    }

    fprintf(f, "{\n");
    for (int i = 0; i < g_position_count; i++) {
        // Escape path for JSON
        fprintf(f, "  \"%s\": %d%s\n",
                g_positions[i].path,
                g_positions[i].position_sec,
                (i < g_position_count - 1) ? "," : "");
    }
    fprintf(f, "}\n");

    fclose(f);
    g_dirty = false;
    printf("[POSITIONS] Saved %d positions\n", g_position_count);
}

void positions_cleanup(void) {
    positions_save();
    g_position_count = 0;
}
