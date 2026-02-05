/**
 * File Menu Implementation
 */

#include "filemenu.h"
#include "metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

// Current file menu state
static char g_target_path[512] = {0};
static char g_target_name[256] = {0};
static bool g_is_directory = false;
static int g_cursor = 0;
static bool g_confirm_pending = false;

// Rename state
static char g_rename_buffer[256] = {0};
static int g_rename_cursor = 0;  // Position in text
static int g_kbd_row = 0;        // Keyboard grid row
static int g_kbd_col = 0;        // Keyboard grid column

// Character set for rename - QWERTY layout grid
#define KBD_COLS 10
#define KBD_ROWS 5
static const char CHARSET[KBD_ROWS][KBD_COLS + 1] = {
    "1234567890",  // Row 0: numbers
    "QWERTYUIOP",  // Row 1: QWERTY top row
    "ASDFGHJKL ",  // Row 2: QWERTY middle row (space at end)
    "ZXCVBNM-._",  // Row 3: QWERTY bottom row + symbols
    " ()[]{}   ",  // Row 4: space and brackets
};

/**
 * Recursively delete directory
 */
static int delete_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *entry;
    char full_path[512];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                delete_directory(full_path);
            } else {
                unlink(full_path);
            }
        }
    }

    closedir(dir);
    return rmdir(path);
}

void filemenu_init(const char *path, bool is_directory) {
    strncpy(g_target_path, path, sizeof(g_target_path) - 1);
    g_is_directory = is_directory;
    g_cursor = 0;
    g_confirm_pending = false;

    // Extract filename from path
    const char *name = strrchr(path, '/');
    if (name) {
        strncpy(g_target_name, name + 1, sizeof(g_target_name) - 1);
    } else {
        strncpy(g_target_name, path, sizeof(g_target_name) - 1);
    }
}

int filemenu_get_cursor(void) {
    return g_cursor;
}

static int get_menu_count(void) {
    if (!g_is_directory) {
        // Files: Rename, Delete, Cancel (3 options)
        return 3;
    }
    // Directories: Rename, Delete, Scan, [Restore if backup], Cancel
    return metadata_has_backup() ? 5 : 4;
}

static int adjust_cursor_for_display(int cursor) {
    if (!g_is_directory) {
        // Files: skip SCAN_METADATA and RESTORE_METADATA
        if (cursor >= 2) return FILEMENU_CANCEL;  // cursor 2 = Cancel
        return cursor;  // 0=Rename, 1=Delete
    }
    // Directories with backup: all options available
    if (metadata_has_backup()) {
        return cursor;
    }
    // Directories without backup: skip RESTORE_METADATA
    if (cursor >= FILEMENU_RESTORE_METADATA) {
        return cursor + 1;  // Skip restore, go to cancel
    }
    return cursor;
}

void filemenu_move_cursor(int delta) {
    int count = get_menu_count();
    g_cursor += delta;
    if (g_cursor < 0) g_cursor = count - 1;
    if (g_cursor >= count) g_cursor = 0;
}

int filemenu_get_actual_option(void) {
    return adjust_cursor_for_display(g_cursor);
}

bool filemenu_select(void) {
    int option = adjust_cursor_for_display(g_cursor);
    switch (option) {
        case FILEMENU_RENAME:
            // Will transition to rename state
            return true;
        case FILEMENU_DELETE:
            g_confirm_pending = true;
            return false;  // Stay in menu, show confirm
        case FILEMENU_SCAN_METADATA:
            // Will transition to scanning state (only for directories)
            return true;
        case FILEMENU_RESTORE_METADATA:
            // Restore from backup
            if (metadata_restore_backup()) {
                printf("[FILEMENU] Metadata restored from backup\n");
            }
            return true;
        case FILEMENU_CANCEL:
            return true;
    }
    return true;
}

bool filemenu_needs_confirm(void) {
    return g_confirm_pending;
}

FileMenuResult filemenu_confirm_delete(bool confirmed) {
    g_confirm_pending = false;

    if (!confirmed) {
        return FILEMENU_RESULT_CANCELLED;
    }

    int result;
    if (g_is_directory) {
        result = delete_directory(g_target_path);
    } else {
        result = unlink(g_target_path);
    }

    if (result == 0) {
        printf("[FILEMENU] Deleted: %s\n", g_target_path);
        return FILEMENU_RESULT_DELETED;
    } else {
        fprintf(stderr, "[FILEMENU] Failed to delete: %s\n", g_target_path);
        return FILEMENU_RESULT_CANCELLED;
    }
}

const char* filemenu_get_filename(void) {
    return g_target_name;
}

const char* filemenu_get_path(void) {
    return g_target_path;
}

bool filemenu_is_directory(void) {
    return g_is_directory;
}

// Rename functionality
void filemenu_rename_init(void) {
    strncpy(g_rename_buffer, g_target_name, sizeof(g_rename_buffer) - 1);
    g_rename_cursor = strlen(g_rename_buffer);
    g_kbd_row = 0;
    g_kbd_col = 0;
}

const char* filemenu_rename_get_text(void) {
    return g_rename_buffer;
}

int filemenu_rename_get_cursor(void) {
    return g_rename_cursor;
}

void filemenu_rename_move_kbd(int dx, int dy) {
    g_kbd_col += dx;
    g_kbd_row += dy;

    // Wrap columns
    if (g_kbd_col < 0) g_kbd_col = KBD_COLS - 1;
    if (g_kbd_col >= KBD_COLS) g_kbd_col = 0;

    // Wrap rows
    if (g_kbd_row < 0) g_kbd_row = KBD_ROWS - 1;
    if (g_kbd_row >= KBD_ROWS) g_kbd_row = 0;
}

void filemenu_rename_move_pos(int delta) {
    g_rename_cursor += delta;
    if (g_rename_cursor < 0) g_rename_cursor = 0;
    int len = strlen(g_rename_buffer);
    if (g_rename_cursor > len) g_rename_cursor = len;
}

void filemenu_rename_insert(void) {
    int len = strlen(g_rename_buffer);
    if (len >= (int)sizeof(g_rename_buffer) - 1) return;

    char c = CHARSET[g_kbd_row][g_kbd_col];
    if (c == '\0') return;  // Invalid position

    // Shift characters to make room
    for (int i = len; i >= g_rename_cursor; i--) {
        g_rename_buffer[i + 1] = g_rename_buffer[i];
    }
    g_rename_buffer[g_rename_cursor] = c;
    g_rename_cursor++;
}

void filemenu_rename_delete(void) {
    int len = strlen(g_rename_buffer);
    if (g_rename_cursor == 0 || len == 0) return;

    // Shift characters to fill gap
    for (int i = g_rename_cursor - 1; i < len; i++) {
        g_rename_buffer[i] = g_rename_buffer[i + 1];
    }
    g_rename_cursor--;
}

char filemenu_rename_get_selected_char(void) {
    char c = CHARSET[g_kbd_row][g_kbd_col];
    if (c == '\0') return ' ';  // Empty position
    return c;
}

void filemenu_rename_get_kbd_pos(int *row, int *col) {
    if (row) *row = g_kbd_row;
    if (col) *col = g_kbd_col;
}

void filemenu_rename_get_kbd_size(int *cols, int *rows) {
    if (cols) *cols = KBD_COLS;
    if (rows) *rows = KBD_ROWS;
}

char filemenu_rename_get_char_at(int row, int col) {
    if (row < 0 || row >= KBD_ROWS || col < 0 || col >= KBD_COLS) {
        return '\0';
    }
    return CHARSET[row][col];
}

FileMenuResult filemenu_rename_confirm(void) {
    if (strlen(g_rename_buffer) == 0) {
        return FILEMENU_RESULT_CANCELLED;
    }

    // Build new path
    char new_path[512];
    char *last_sep = strrchr(g_target_path, '/');
    if (last_sep) {
        int dir_len = last_sep - g_target_path;
        strncpy(new_path, g_target_path, dir_len);
        new_path[dir_len] = '\0';
        snprintf(new_path + dir_len, sizeof(new_path) - dir_len, "/%s", g_rename_buffer);
    } else {
        strncpy(new_path, g_rename_buffer, sizeof(new_path) - 1);
    }

    // Rename file
    if (rename(g_target_path, new_path) == 0) {
        printf("[FILEMENU] Renamed: %s -> %s\n", g_target_path, new_path);
        return FILEMENU_RESULT_RENAMED;
    } else {
        fprintf(stderr, "[FILEMENU] Failed to rename: %s\n", g_target_path);
        return FILEMENU_RESULT_CANCELLED;
    }
}
