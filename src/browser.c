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
 * Check if filename has an audio extension (optimized)
 */
static bool is_audio_file(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext || ext[1] == '\0') return false;

    // Get extension length
    int len = 0;
    for (const char *p = ext; *p; p++) len++;
    if (len < 4 || len > 5) return false;

    // Case-insensitive comparison
    char c1 = ext[1] | 0x20;  // tolower
    char c2 = ext[2] | 0x20;
    char c3 = ext[3] | 0x20;

    if (len == 4) {
        // .mp3, .ogg, .wav, .m4a
        if (c1 == 'm' && c2 == 'p' && c3 == '3') return true;
        if (c1 == 'o' && c2 == 'g' && c3 == 'g') return true;
        if (c1 == 'w' && c2 == 'a' && c3 == 'v') return true;
        if (c1 == 'm' && c2 == '4' && c3 == 'a') return true;
    } else {
        // .flac, .webm, .opus
        char c4 = ext[4] | 0x20;
        if (c1 == 'f' && c2 == 'l' && c3 == 'a' && c4 == 'c') return true;
        if (c1 == 'w' && c2 == 'e' && c3 == 'b' && c4 == 'm') return true;
        if (c1 == 'o' && c2 == 'p' && c3 == 'u' && c4 == 's') return true;
    }

    return false;
}

/**
 * Natural sort comparison - treats embedded numbers as integers
 * Example: "Track 2" < "Track 10" (unlike alphabetical sort)
 */
static int compare_natural(const char *a, const char *b) {
    while (*a && *b) {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            // Both pointing to digits - compare as integers
            long na = strtol(a, (char **)&a, 10);
            long nb = strtol(b, (char **)&b, 10);
            if (na != nb) return (na < nb) ? -1 : 1;
            // Numbers equal, continue comparing rest of string
        } else {
            // Compare characters case-insensitively
            int ca = tolower((unsigned char)*a);
            int cb = tolower((unsigned char)*b);
            if (ca != cb) return ca - cb;
            a++;
            b++;
        }
    }
    // Handle different lengths
    return (unsigned char)*a - (unsigned char)*b;
}

/**
 * Compare function for sorting entries (parent first, then directories, then files)
 */
static int compare_entries(const void *a, const void *b) {
    const FileEntry *ea = (const FileEntry *)a;
    const FileEntry *eb = (const FileEntry *)b;

    // Parent entry always first
    if (ea->type == ENTRY_PARENT) return -1;
    if (eb->type == ENTRY_PARENT) return 1;

    // Directories come before files
    if (ea->type != eb->type) {
        return ea->type == ENTRY_DIRECTORY ? -1 : 1;
    }

    // Natural sort within same type
    return compare_natural(ea->name, eb->name);
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

    // Add ".." entry if not at base path
    if (strcmp(path, g_base_path) != 0) {
        FileEntry *parent = &g_entries[g_entry_count];
        parent->type = ENTRY_PARENT;
        strncpy(parent->name, "", sizeof(parent->name) - 1);  // Empty, prefix handles display
        // Build parent path
        char parent_path[512];
        strncpy(parent_path, path, sizeof(parent_path) - 1);
        char *last_sep = strrchr(parent_path, '/');
        if (last_sep && last_sep != parent_path) {
            *last_sep = '\0';
        }
        strncpy(parent->full_path, parent_path, sizeof(parent->full_path) - 1);
        g_entry_count++;
    }

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

    if (entry->type == ENTRY_PARENT) {
        // Go up to parent directory
        browser_go_up();
        return false;
    } else if (entry->type == ENTRY_DIRECTORY) {
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

void browser_set_cursor(int pos) {
    if (pos < 0) pos = 0;
    if (pos >= g_entry_count) pos = g_entry_count > 0 ? g_entry_count - 1 : 0;

    g_cursor = pos;

    // Update scroll offset to keep cursor visible
    if (g_cursor < g_scroll_offset) {
        g_scroll_offset = g_cursor;
    } else if (g_cursor >= g_scroll_offset + VISIBLE_ITEMS) {
        g_scroll_offset = g_cursor - VISIBLE_ITEMS + 1;
    }
}

int browser_navigate_to(const char *path) {
    if (!path || path[0] == '\0') return -1;

    // Verify directory exists
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Cannot navigate to: %s\n", path);
        return -1;
    }
    closedir(dir);

    strncpy(g_current_path, path, sizeof(g_current_path) - 1);
    return scan_directory(g_current_path);
}
