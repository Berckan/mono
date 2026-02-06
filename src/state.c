/**
 * State Persistence Implementation
 *
 * Simple JSON-based state persistence using manual string parsing
 * (no external JSON library dependency).
 *
 * File format:
 * {
 *   "last_file": "/path/to/file.mp3",
 *   "last_folder": "/path/to",
 *   "last_position": 145,
 *   "last_cursor": 3,
 *   "volume": 80,
 *   "shuffle": false,
 *   "repeat": 0,
 *   "was_playing": true
 * }
 */

#include "state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// Data directory path
#ifdef __APPLE__
// macOS: use ~/.mono for testing
#define DATA_DIR_BASE "/.mono"
#else
// Trimui Brick: use standard userdata path
#define DATA_DIR_BASE "/.userdata/tg5040/Mono"
#endif

#define STATE_FILENAME "state.json"

static char g_data_dir[512] = {0};
static char g_state_path[512] = {0};

/**
 * Create directory recursively (like mkdir -p)
 */
static int mkdir_p(const char *path) {
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    // Remove trailing slash
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    // Create each directory in path
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }

    // Create final directory
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

/**
 * Extract string value from JSON (simple parser)
 * Finds "key": "value" and copies value to dest
 */
static bool json_get_string(const char *json, const char *key, char *dest, size_t size) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char *start = strstr(json, pattern);
    if (!start) return false;

    start += strlen(pattern);

    // Skip whitespace
    while (*start == ' ' || *start == '\t') start++;

    if (*start != '"') return false;
    start++;

    // Find end quote
    const char *end = strchr(start, '"');
    if (!end) return false;

    size_t len = end - start;
    if (len >= size) len = size - 1;

    strncpy(dest, start, len);
    dest[len] = '\0';

    return true;
}

/**
 * Extract integer value from JSON
 * Finds "key": 123 and returns value
 */
static bool json_get_int(const char *json, const char *key, int *value) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char *start = strstr(json, pattern);
    if (!start) return false;

    start += strlen(pattern);

    // Skip whitespace
    while (*start == ' ' || *start == '\t') start++;

    *value = atoi(start);
    return true;
}

/**
 * Extract boolean value from JSON
 * Finds "key": true/false and returns value
 */
static bool json_get_bool(const char *json, const char *key, bool *value) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char *start = strstr(json, pattern);
    if (!start) return false;

    start += strlen(pattern);

    // Skip whitespace
    while (*start == ' ' || *start == '\t') start++;

    *value = (strncmp(start, "true", 4) == 0);
    return true;
}

/**
 * Escape string for JSON output
 */
static void json_escape_string(const char *src, char *dest, size_t size) {
    size_t di = 0;
    for (size_t si = 0; src[si] && di < size - 2; si++) {
        char c = src[si];
        if (c == '"' || c == '\\') {
            if (di < size - 3) {
                dest[di++] = '\\';
                dest[di++] = c;
            }
        } else if (c == '\n') {
            if (di < size - 3) {
                dest[di++] = '\\';
                dest[di++] = 'n';
            }
        } else {
            dest[di++] = c;
        }
    }
    dest[di] = '\0';
}

int state_init(void) {
    // Build data directory path
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[STATE] HOME not set\n");
        return -1;
    }

    snprintf(g_data_dir, sizeof(g_data_dir), "%s%s", home, DATA_DIR_BASE);
    snprintf(g_state_path, sizeof(g_state_path), "%s/%s", g_data_dir, STATE_FILENAME);

    // Create data directory if needed
    if (mkdir_p(g_data_dir) != 0) {
        fprintf(stderr, "[STATE] Failed to create data dir: %s\n", g_data_dir);
        return -1;
    }

    printf("[STATE] Data dir: %s\n", g_data_dir);
    return 0;
}

void state_cleanup(void) {
    // Nothing to clean up currently
}

bool state_save(const AppStateData *data) {
    if (!data) return false;

    FILE *f = fopen(g_state_path, "w");
    if (!f) {
        fprintf(stderr, "[STATE] Failed to open %s for writing\n", g_state_path);
        return false;
    }

    // Escape strings for JSON
    char escaped_file[1024];
    char escaped_folder[1024];
    json_escape_string(data->last_file, escaped_file, sizeof(escaped_file));
    json_escape_string(data->last_folder, escaped_folder, sizeof(escaped_folder));

    fprintf(f, "{\n");
    fprintf(f, "  \"last_file\": \"%s\",\n", escaped_file);
    fprintf(f, "  \"last_folder\": \"%s\",\n", escaped_folder);
    fprintf(f, "  \"last_position\": %d,\n", data->last_position);
    fprintf(f, "  \"last_cursor\": %d,\n", data->last_cursor);
    fprintf(f, "  \"volume\": %d,\n", data->volume);
    fprintf(f, "  \"shuffle\": %s,\n", data->shuffle ? "true" : "false");
    fprintf(f, "  \"repeat\": %d,\n", (int)data->repeat);
    fprintf(f, "  \"theme\": %d,\n", (int)data->theme);
    fprintf(f, "  \"power_mode\": %d,\n", (int)data->power_mode);
    fprintf(f, "  \"eq_band_0\": %d,\n", data->eq_bands[0]);
    fprintf(f, "  \"eq_band_1\": %d,\n", data->eq_bands[1]);
    fprintf(f, "  \"eq_band_2\": %d,\n", data->eq_bands[2]);
    fprintf(f, "  \"eq_band_3\": %d,\n", data->eq_bands[3]);
    fprintf(f, "  \"eq_band_4\": %d,\n", data->eq_bands[4]);
    fprintf(f, "  \"was_playing\": %s\n", data->was_playing ? "true" : "false");
    fprintf(f, "}\n");

    fclose(f);
    printf("[STATE] Saved state to %s\n", g_state_path);
    return true;
}

bool state_load(AppStateData *data) {
    if (!data) return false;

    // Initialize with defaults
    memset(data, 0, sizeof(AppStateData));
    data->volume = 80;
    data->shuffle = false;
    data->repeat = REPEAT_OFF;
    data->theme = THEME_DARK;
    data->power_mode = POWER_MODE_BALANCED;
    memset(data->eq_bands, 0, sizeof(data->eq_bands));
    data->has_resume_data = false;

    FILE *f = fopen(g_state_path, "r");
    if (!f) {
        printf("[STATE] No saved state found\n");
        return false;
    }

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 65536) {
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

    // Parse JSON
    json_get_string(json, "last_file", data->last_file, sizeof(data->last_file));
    json_get_string(json, "last_folder", data->last_folder, sizeof(data->last_folder));
    json_get_int(json, "last_position", &data->last_position);
    json_get_int(json, "last_cursor", &data->last_cursor);
    json_get_int(json, "volume", &data->volume);
    json_get_bool(json, "shuffle", &data->shuffle);

    int repeat_int = 0;
    if (json_get_int(json, "repeat", &repeat_int)) {
        data->repeat = (RepeatMode)repeat_int;
    }

    int theme_int = 0;
    if (json_get_int(json, "theme", &theme_int)) {
        data->theme = (ThemeId)theme_int;
    }

    int power_mode_int = 1;  // Default to BALANCED
    if (json_get_int(json, "power_mode", &power_mode_int)) {
        data->power_mode = (PowerMode)power_mode_int;
    }

    // Load 5-band EQ (with backwards compat for old eq_bass/eq_treble)
    json_get_int(json, "eq_band_0", &data->eq_bands[0]);
    json_get_int(json, "eq_band_1", &data->eq_bands[1]);
    json_get_int(json, "eq_band_2", &data->eq_bands[2]);
    json_get_int(json, "eq_band_3", &data->eq_bands[3]);
    json_get_int(json, "eq_band_4", &data->eq_bands[4]);
    // Backwards compat: old files had eq_bass (band 0) and eq_treble (band 4)
    if (!strstr(json, "eq_band_0")) {
        json_get_int(json, "eq_bass", &data->eq_bands[0]);
        json_get_int(json, "eq_treble", &data->eq_bands[4]);
    }

    json_get_bool(json, "was_playing", &data->was_playing);

    free(json);

    // Validate loaded data
    if (strlen(data->last_file) > 0) {
        // Check if file still exists
        FILE *check = fopen(data->last_file, "r");
        if (check) {
            fclose(check);
            data->has_resume_data = true;
            printf("[STATE] Loaded state: %s @ %ds\n", data->last_file, data->last_position);
        } else {
            printf("[STATE] Last file no longer exists: %s\n", data->last_file);
            data->last_file[0] = '\0';
        }
    }

    return data->has_resume_data;
}

void state_clear(void) {
    if (remove(g_state_path) == 0) {
        printf("[STATE] Cleared saved state\n");
    }
}

const char* state_get_data_dir(void) {
    return g_data_dir;
}

// Settings changed callback
static SettingsChangedCallback g_settings_callback = NULL;

void state_set_settings_callback(SettingsChangedCallback callback) {
    g_settings_callback = callback;
}

void state_notify_settings_changed(void) {
    if (g_settings_callback) {
        g_settings_callback();
    }
}
