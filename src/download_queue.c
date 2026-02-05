/**
 * Background Download Queue Implementation
 *
 * Uses pthread for background downloads. Downloads run sequentially
 * in a worker thread while the main thread continues UI updates.
 */

#include "download_queue.h"
#include "youtube.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Queue storage
static DownloadItem g_queue[DOWNLOAD_QUEUE_MAX];
static int g_queue_count = 0;
static int g_current_index = -1;  // Index of currently downloading item

// Thread synchronization
static pthread_t g_worker_thread;
static pthread_mutex_t g_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_queue_cond = PTHREAD_COND_INITIALIZER;
static bool g_worker_running = false;
static bool g_shutdown_requested = false;

// Completion tracking
static bool g_has_new_completions = false;
static char g_last_completed_path[512] = {0};

// Current download progress (updated from worker thread)
static int g_current_progress = 0;

// View state (cursor/scroll for queue list view)
static int g_view_cursor = 0;
static int g_view_scroll = 0;
#define VIEW_VISIBLE_ITEMS 8

/**
 * Progress callback for youtube_download
 * Called from worker thread
 */
static bool worker_progress_callback(int percent, const char *status) {
    (void)status;  // Unused for now

    pthread_mutex_lock(&g_queue_mutex);
    g_current_progress = percent;
    if (g_current_index >= 0 && g_current_index < g_queue_count) {
        g_queue[g_current_index].progress = percent;
    }
    pthread_mutex_unlock(&g_queue_mutex);

    // Check if shutdown requested
    return !g_shutdown_requested;
}

/**
 * Worker thread function
 * Processes downloads from queue sequentially
 */
static void* worker_thread_func(void *arg) {
    (void)arg;

    printf("[DLQUEUE] Worker thread started\n");

    while (1) {
        pthread_mutex_lock(&g_queue_mutex);

        // Wait for work or shutdown
        while (!g_shutdown_requested) {
            // Find next pending item
            int next_pending = -1;
            for (int i = 0; i < g_queue_count; i++) {
                if (g_queue[i].status == DL_PENDING) {
                    next_pending = i;
                    break;
                }
            }

            if (next_pending >= 0) {
                // Found work
                g_current_index = next_pending;
                g_queue[next_pending].status = DL_DOWNLOADING;
                g_queue[next_pending].progress = 0;
                g_current_progress = 0;
                break;
            }

            // No work, wait for signal
            pthread_cond_wait(&g_queue_cond, &g_queue_mutex);
        }

        if (g_shutdown_requested) {
            pthread_mutex_unlock(&g_queue_mutex);
            break;
        }

        // Get item info (copy to local vars while holding lock)
        char video_id[16];
        char title[256];
        strncpy(video_id, g_queue[g_current_index].video_id, sizeof(video_id) - 1);
        video_id[sizeof(video_id) - 1] = '\0';
        strncpy(title, g_queue[g_current_index].title, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';

        printf("[DLQUEUE] Starting download: %s - %s\n", video_id, title);

        pthread_mutex_unlock(&g_queue_mutex);

        // Perform download (blocking, but in worker thread)
        const char *path = youtube_download(video_id, title, worker_progress_callback);

        // Update item status
        pthread_mutex_lock(&g_queue_mutex);

        if (g_current_index >= 0 && g_current_index < g_queue_count) {
            if (path) {
                g_queue[g_current_index].status = DL_COMPLETE;
                g_queue[g_current_index].progress = 100;
                strncpy(g_queue[g_current_index].filepath, path,
                        sizeof(g_queue[g_current_index].filepath) - 1);
                strncpy(g_last_completed_path, path, sizeof(g_last_completed_path) - 1);
                g_has_new_completions = true;
                printf("[DLQUEUE] Download complete: %s\n", path);
            } else {
                g_queue[g_current_index].status = DL_FAILED;
                const char *err = youtube_get_error();
                if (err) {
                    strncpy(g_queue[g_current_index].error, err,
                            sizeof(g_queue[g_current_index].error) - 1);
                }
                printf("[DLQUEUE] Download failed: %s\n", err ? err : "Unknown error");
            }
        }

        g_current_index = -1;
        g_current_progress = 0;

        pthread_mutex_unlock(&g_queue_mutex);

        // Small delay between downloads
        usleep(100000);  // 100ms
    }

    printf("[DLQUEUE] Worker thread exiting\n");
    return NULL;
}

void dlqueue_init(void) {
    pthread_mutex_lock(&g_queue_mutex);

    // Clear queue
    memset(g_queue, 0, sizeof(g_queue));
    g_queue_count = 0;
    g_current_index = -1;
    g_current_progress = 0;
    g_has_new_completions = false;
    g_last_completed_path[0] = '\0';
    g_shutdown_requested = false;

    pthread_mutex_unlock(&g_queue_mutex);

    // Start worker thread
    if (!g_worker_running) {
        if (pthread_create(&g_worker_thread, NULL, worker_thread_func, NULL) == 0) {
            g_worker_running = true;
            printf("[DLQUEUE] Initialized with worker thread\n");
        } else {
            fprintf(stderr, "[DLQUEUE] Failed to create worker thread\n");
        }
    }
}

void dlqueue_shutdown(void) {
    if (!g_worker_running) return;

    printf("[DLQUEUE] Shutting down...\n");

    pthread_mutex_lock(&g_queue_mutex);
    g_shutdown_requested = true;
    pthread_cond_signal(&g_queue_cond);
    pthread_mutex_unlock(&g_queue_mutex);

    pthread_join(g_worker_thread, NULL);
    g_worker_running = false;

    printf("[DLQUEUE] Shutdown complete\n");
}

bool dlqueue_add(const char *video_id, const char *title, const char *channel) {
    if (!video_id || !title) return false;

    pthread_mutex_lock(&g_queue_mutex);

    // Check if queue full
    if (g_queue_count >= DOWNLOAD_QUEUE_MAX) {
        pthread_mutex_unlock(&g_queue_mutex);
        printf("[DLQUEUE] Queue full, cannot add: %s\n", title);
        return false;
    }

    // Check for duplicates (same video_id pending or downloading)
    for (int i = 0; i < g_queue_count; i++) {
        if ((g_queue[i].status == DL_PENDING || g_queue[i].status == DL_DOWNLOADING) &&
            strcmp(g_queue[i].video_id, video_id) == 0) {
            pthread_mutex_unlock(&g_queue_mutex);
            printf("[DLQUEUE] Already in queue: %s\n", video_id);
            return false;
        }
    }

    // Add to queue
    DownloadItem *item = &g_queue[g_queue_count];
    memset(item, 0, sizeof(DownloadItem));
    strncpy(item->video_id, video_id, sizeof(item->video_id) - 1);
    strncpy(item->title, title, sizeof(item->title) - 1);
    if (channel) {
        strncpy(item->channel, channel, sizeof(item->channel) - 1);
    }
    item->status = DL_PENDING;
    item->progress = 0;

    g_queue_count++;

    printf("[DLQUEUE] Added to queue (%d items): %s - %s\n",
           g_queue_count, video_id, title);

    // Signal worker thread
    pthread_cond_signal(&g_queue_cond);

    pthread_mutex_unlock(&g_queue_mutex);

    return true;
}

int dlqueue_pending_count(void) {
    pthread_mutex_lock(&g_queue_mutex);

    int count = 0;
    for (int i = 0; i < g_queue_count; i++) {
        if (g_queue[i].status == DL_PENDING || g_queue[i].status == DL_DOWNLOADING) {
            count++;
        }
    }

    pthread_mutex_unlock(&g_queue_mutex);
    return count;
}

int dlqueue_total_count(void) {
    pthread_mutex_lock(&g_queue_mutex);
    int count = g_queue_count;
    pthread_mutex_unlock(&g_queue_mutex);
    return count;
}

bool dlqueue_is_downloading(void) {
    pthread_mutex_lock(&g_queue_mutex);
    bool downloading = (g_current_index >= 0);
    pthread_mutex_unlock(&g_queue_mutex);
    return downloading;
}

int dlqueue_get_progress(void) {
    pthread_mutex_lock(&g_queue_mutex);
    int progress = g_current_index >= 0 ? g_current_progress : -1;
    pthread_mutex_unlock(&g_queue_mutex);
    return progress;
}

const char* dlqueue_get_current_title(void) {
    static char title[256];

    pthread_mutex_lock(&g_queue_mutex);

    if (g_current_index >= 0 && g_current_index < g_queue_count) {
        strncpy(title, g_queue[g_current_index].title, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';
        pthread_mutex_unlock(&g_queue_mutex);
        return title;
    }

    pthread_mutex_unlock(&g_queue_mutex);
    return NULL;
}

const DownloadItem* dlqueue_get_item(int index) {
    pthread_mutex_lock(&g_queue_mutex);

    if (index < 0 || index >= g_queue_count) {
        pthread_mutex_unlock(&g_queue_mutex);
        return NULL;
    }

    // Return pointer (caller should not hold long)
    const DownloadItem *item = &g_queue[index];
    pthread_mutex_unlock(&g_queue_mutex);
    return item;
}

void dlqueue_clear_completed(void) {
    pthread_mutex_lock(&g_queue_mutex);

    // Compact queue, removing completed/failed items
    int write_idx = 0;
    for (int read_idx = 0; read_idx < g_queue_count; read_idx++) {
        if (g_queue[read_idx].status == DL_PENDING ||
            g_queue[read_idx].status == DL_DOWNLOADING) {
            if (write_idx != read_idx) {
                memcpy(&g_queue[write_idx], &g_queue[read_idx], sizeof(DownloadItem));
            }
            write_idx++;
        }
    }

    g_queue_count = write_idx;

    // Adjust current_index if needed
    if (g_current_index >= g_queue_count) {
        g_current_index = -1;
    }

    pthread_mutex_unlock(&g_queue_mutex);

    printf("[DLQUEUE] Cleared completed, %d items remaining\n", g_queue_count);
}

const char* dlqueue_get_last_completed(void) {
    pthread_mutex_lock(&g_queue_mutex);
    const char *path = g_last_completed_path[0] ? g_last_completed_path : NULL;
    pthread_mutex_unlock(&g_queue_mutex);
    return path;
}

bool dlqueue_has_new_completions(void) {
    pthread_mutex_lock(&g_queue_mutex);
    bool has_new = g_has_new_completions;
    g_has_new_completions = false;  // Reset flag
    pthread_mutex_unlock(&g_queue_mutex);
    return has_new;
}

bool dlqueue_is_queued(const char *video_id) {
    if (!video_id) return false;

    pthread_mutex_lock(&g_queue_mutex);

    bool found = false;
    for (int i = 0; i < g_queue_count; i++) {
        if ((g_queue[i].status == DL_PENDING || g_queue[i].status == DL_DOWNLOADING) &&
            strcmp(g_queue[i].video_id, video_id) == 0) {
            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&g_queue_mutex);
    return found;
}

// ============================================================================
// View State Management
// ============================================================================

void dlqueue_view_init(void) {
    pthread_mutex_lock(&g_queue_mutex);
    g_view_cursor = 0;
    g_view_scroll = 0;
    pthread_mutex_unlock(&g_queue_mutex);
}

int dlqueue_view_get_cursor(void) {
    pthread_mutex_lock(&g_queue_mutex);
    int cursor = g_view_cursor;
    pthread_mutex_unlock(&g_queue_mutex);
    return cursor;
}

void dlqueue_view_move_cursor(int delta) {
    pthread_mutex_lock(&g_queue_mutex);

    if (g_queue_count == 0) {
        pthread_mutex_unlock(&g_queue_mutex);
        return;
    }

    g_view_cursor += delta;

    // Clamp cursor
    if (g_view_cursor < 0) g_view_cursor = 0;
    if (g_view_cursor >= g_queue_count) g_view_cursor = g_queue_count - 1;

    // Adjust scroll to keep cursor visible
    if (g_view_cursor < g_view_scroll) {
        g_view_scroll = g_view_cursor;
    }
    if (g_view_cursor >= g_view_scroll + VIEW_VISIBLE_ITEMS) {
        g_view_scroll = g_view_cursor - VIEW_VISIBLE_ITEMS + 1;
    }

    pthread_mutex_unlock(&g_queue_mutex);
}

int dlqueue_view_get_scroll_offset(void) {
    pthread_mutex_lock(&g_queue_mutex);
    int scroll = g_view_scroll;
    pthread_mutex_unlock(&g_queue_mutex);
    return scroll;
}

bool dlqueue_view_action_select(void) {
    pthread_mutex_lock(&g_queue_mutex);

    if (g_view_cursor < 0 || g_view_cursor >= g_queue_count) {
        pthread_mutex_unlock(&g_queue_mutex);
        return false;
    }

    DownloadItem *item = &g_queue[g_view_cursor];
    bool playable = (item->status == DL_COMPLETE && item->filepath[0] != '\0');

    pthread_mutex_unlock(&g_queue_mutex);
    return playable;
}

const char* dlqueue_view_get_selected_path(void) {
    static char path[512];

    pthread_mutex_lock(&g_queue_mutex);

    if (g_view_cursor < 0 || g_view_cursor >= g_queue_count) {
        pthread_mutex_unlock(&g_queue_mutex);
        return NULL;
    }

    DownloadItem *item = &g_queue[g_view_cursor];
    if (item->status != DL_COMPLETE || item->filepath[0] == '\0') {
        pthread_mutex_unlock(&g_queue_mutex);
        return NULL;
    }

    strncpy(path, item->filepath, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    pthread_mutex_unlock(&g_queue_mutex);
    return path;
}

bool dlqueue_view_action_cancel(void) {
    return dlqueue_cancel(g_view_cursor);
}

bool dlqueue_cancel(int index) {
    pthread_mutex_lock(&g_queue_mutex);

    if (index < 0 || index >= g_queue_count) {
        pthread_mutex_unlock(&g_queue_mutex);
        return false;
    }

    DownloadItem *item = &g_queue[index];

    // Only cancel pending items (can't interrupt active download safely)
    if (item->status == DL_PENDING) {
        item->status = DL_FAILED;
        strncpy(item->error, "Cancelled by user", sizeof(item->error) - 1);
        printf("[DLQUEUE] Cancelled: %s\n", item->title);
        pthread_mutex_unlock(&g_queue_mutex);
        return true;
    }

    pthread_mutex_unlock(&g_queue_mutex);
    return false;
}
