/**
 * File Browser Implementation
 *
 * Provides directory navigation with filtering for audio files.
 * Supports MP3, FLAC, OGG, and WAV formats.
 */

#include "browser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

// Maximum entries we can handle
#define MAX_ENTRIES 1024

// Visible items in list (for scroll calculation)
#define VISIBLE_ITEMS 8

// File list
static FileEntry g_entries[MAX_ENTRIES];
static int g_entry_count = 0;
static int g_cursor = 0;
static int g_scroll_offset = 0;

// Current paths
static char g_base_path[512];
static char g_current_path[512];

/**
 * Check if filename has an audio extension
 */
static bool is_audio_file(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return false;

    // Convert extension to lowercase for comparison
    char lower_ext[16];
    int i;
    for (i = 0; ext[i] && i < 15; i++) {
        lower_ext[i] = tolower((unsigned char)ext[i]);
    }
    lower_ext[i] = '\0';

    return strcmp(lower_ext, ".mp3") == 0 ||
           strcmp(lower_ext, ".flac") == 0 ||
           strcmp(lower_ext, ".ogg") == 0 ||
           strcmp(lower_ext, ".wav") == 0;
}

/**
 * Compare function for sorting entries (directories first, then alphabetically)
 */
static int compare_entries(const void *a, const void *b) {
    const FileEntry *ea = (const FileEntry *)a;
    const FileEntry *eb = (const FileEntry *)b;

    // Directories come first
    if (ea->type != eb->type) {
        return ea->type == ENTRY_DIRECTORY ? -1 : 1;
    }

    // Alphabetical within same type (case-insensitive)
    return strcasecmp(ea->name, eb->name);
}

/**
 * Scan directory and populate entries
 */
static int scan_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Cannot open directory: %s\n", path);
        return -1;
    }

    g_entry_count = 0;
    g_cursor = 0;
    g_scroll_offset = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && g_entry_count < MAX_ENTRIES) {
        // Skip hidden files and special entries
        if (entry->d_name[0] == '.') continue;

        // Build full path
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Get file info
        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        FileEntry *fe = &g_entries[g_entry_count];

        if (S_ISDIR(st.st_mode)) {
            // Directory
            fe->type = ENTRY_DIRECTORY;
            strncpy(fe->name, entry->d_name, sizeof(fe->name) - 1);
            strncpy(fe->full_path, full_path, sizeof(fe->full_path) - 1);
            g_entry_count++;
        } else if (S_ISREG(st.st_mode) && is_audio_file(entry->d_name)) {
            // Audio file
            fe->type = ENTRY_FILE;
            strncpy(fe->name, entry->d_name, sizeof(fe->name) - 1);
            strncpy(fe->full_path, full_path, sizeof(fe->full_path) - 1);
            g_entry_count++;
        }
    }

    closedir(dir);

    // Sort entries
    if (g_entry_count > 0) {
        qsort(g_entries, g_entry_count, sizeof(FileEntry), compare_entries);
    }

    printf("Scanned %s: %d entries\n", path, g_entry_count);

    return 0;
}

int browser_init(const char *base_path) {
    strncpy(g_base_path, base_path, sizeof(g_base_path) - 1);
    strncpy(g_current_path, base_path, sizeof(g_current_path) - 1);

    return scan_directory(g_current_path);
}

void browser_cleanup(void) {
    g_entry_count = 0;
    g_cursor = 0;
}

bool browser_move_cursor(int delta) {
    if (g_entry_count == 0) return false;

    int new_cursor = g_cursor + delta;

    // Clamp to valid range
    if (new_cursor < 0) new_cursor = 0;
    if (new_cursor >= g_entry_count) new_cursor = g_entry_count - 1;

    if (new_cursor == g_cursor) return false;

    g_cursor = new_cursor;

    // Update scroll offset to keep cursor visible
    if (g_cursor < g_scroll_offset) {
        g_scroll_offset = g_cursor;
    } else if (g_cursor >= g_scroll_offset + VISIBLE_ITEMS) {
        g_scroll_offset = g_cursor - VISIBLE_ITEMS + 1;
    }

    return true;
}

bool browser_select_current(void) {
    if (g_entry_count == 0 || g_cursor >= g_entry_count) {
        return false;
    }

    FileEntry *entry = &g_entries[g_cursor];

    if (entry->type == ENTRY_DIRECTORY) {
        // Enter directory
        strncpy(g_current_path, entry->full_path, sizeof(g_current_path) - 1);
        scan_directory(g_current_path);
        return false;  // Not a file selection
    } else {
        // File selected
        return true;
    }
}

bool browser_go_up(void) {
    // Check if we're at the base path
    if (strcmp(g_current_path, g_base_path) == 0) {
        return false;  // Can't go higher than base
    }

    // Find last separator
    char *last_sep = strrchr(g_current_path, '/');
    if (!last_sep || last_sep == g_current_path) {
        return false;
    }

    // Truncate to parent directory
    *last_sep = '\0';

    // Make sure we don't go above base
    if (strlen(g_current_path) < strlen(g_base_path)) {
        strncpy(g_current_path, g_base_path, sizeof(g_current_path) - 1);
    }

    scan_directory(g_current_path);
    return true;
}

int browser_get_cursor(void) {
    return g_cursor;
}

int browser_get_count(void) {
    return g_entry_count;
}

const FileEntry* browser_get_entry(int index) {
    if (index < 0 || index >= g_entry_count) {
        return NULL;
    }
    return &g_entries[index];
}

const char* browser_get_selected_path(void) {
    if (g_cursor >= 0 && g_cursor < g_entry_count) {
        return g_entries[g_cursor].full_path;
    }
    return NULL;
}

const char* browser_get_current_path(void) {
    return g_current_path;
}

int browser_get_scroll_offset(void) {
    return g_scroll_offset;
}
