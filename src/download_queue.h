/**
 * Background Download Queue System
 *
 * Manages a queue of YouTube downloads that run in a background thread.
 * Allows user to continue browsing/searching while downloads complete.
 */

#ifndef DOWNLOAD_QUEUE_H
#define DOWNLOAD_QUEUE_H

#include <stdbool.h>

// Maximum items in queue
#define DOWNLOAD_QUEUE_MAX 20

// Queue item status
typedef enum {
    DL_PENDING,     // Waiting to start
    DL_DOWNLOADING, // Currently downloading
    DL_COMPLETE,    // Finished successfully
    DL_FAILED       // Download failed
} DownloadStatus;

// Queue item
typedef struct {
    char video_id[16];
    char title[256];
    char channel[128];
    DownloadStatus status;
    int progress;           // 0-100
    char error[128];        // Error message if failed
    char filepath[512];     // Path to downloaded file (when complete)
} DownloadItem;

/**
 * Initialize the download queue system
 * Starts the background worker thread
 */
void dlqueue_init(void);

/**
 * Shutdown the download queue
 * Waits for current download to finish, clears queue
 */
void dlqueue_shutdown(void);

/**
 * Add item to download queue
 * @param video_id YouTube video ID
 * @param title Video title
 * @param channel Channel name
 * @return true if added, false if queue full
 */
bool dlqueue_add(const char *video_id, const char *title, const char *channel);

/**
 * Get number of items in queue (pending + downloading)
 */
int dlqueue_pending_count(void);

/**
 * Get total items in queue (including completed)
 */
int dlqueue_total_count(void);

/**
 * Check if currently downloading
 */
bool dlqueue_is_downloading(void);

/**
 * Get current download progress (0-100)
 * Returns -1 if not downloading
 */
int dlqueue_get_progress(void);

/**
 * Get current download title
 * @return Title string or NULL if not downloading
 */
const char* dlqueue_get_current_title(void);

/**
 * Get item at index
 * @param index Queue index (0 = oldest)
 * @return Pointer to item or NULL if invalid index
 */
const DownloadItem* dlqueue_get_item(int index);

/**
 * Clear completed/failed items from queue
 */
void dlqueue_clear_completed(void);

/**
 * Get path of most recently completed download
 * @return File path or NULL if none completed
 */
const char* dlqueue_get_last_completed(void);

/**
 * Check if there are newly completed downloads
 * Resets flag after calling
 * @return true if downloads completed since last check
 */
bool dlqueue_has_new_completions(void);

/**
 * Check if a video is in queue (pending or downloading)
 * @param video_id YouTube video ID
 * @return true if in queue
 */
bool dlqueue_is_queued(const char *video_id);

// ============================================================================
// View State Management (for STATE_DOWNLOAD_QUEUE)
// ============================================================================

/**
 * Initialize view state (call when entering queue view)
 */
void dlqueue_view_init(void);

/**
 * Get current cursor position in queue view
 */
int dlqueue_view_get_cursor(void);

/**
 * Move cursor up/down in queue view
 * @param delta Direction (-1 = up, +1 = down)
 */
void dlqueue_view_move_cursor(int delta);

/**
 * Get scroll offset for visible window
 */
int dlqueue_view_get_scroll_offset(void);

/**
 * Action: Play completed item at cursor
 * @return true if playable file path is set
 */
bool dlqueue_view_action_select(void);

/**
 * Get filepath of item at cursor (if completed)
 * @return File path or NULL
 */
const char* dlqueue_view_get_selected_path(void);

/**
 * Action: Cancel item at cursor (if pending/downloading)
 * @return true if cancelled
 */
bool dlqueue_view_action_cancel(void);

/**
 * Cancel download at specific index
 * @param index Queue index
 * @return true if cancelled
 */
bool dlqueue_cancel(int index);

#endif // DOWNLOAD_QUEUE_H
