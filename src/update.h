/**
 * Self-Update Module
 *
 * Provides ability to check for updates from GitHub releases,
 * download new binary, and apply update on next restart.
 */

#ifndef UPDATE_H
#define UPDATE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Update state machine states
 */
typedef enum {
    UPDATE_IDLE,            // Not checking, ready to start
    UPDATE_CHECKING,        // Querying GitHub API
    UPDATE_AVAILABLE,       // New version found, waiting for user
    UPDATE_DOWNLOADING,     // Downloading binary
    UPDATE_READY,           // Download complete, ready to apply
    UPDATE_UP_TO_DATE,      // Already on latest version
    UPDATE_ERROR            // Error occurred
} UpdateState;

/**
 * Information about an available update
 */
typedef struct {
    char version[32];           // e.g. "v1.7.0"
    char download_url[512];     // Direct URL to binary
    char changelog[2048];       // Release notes (body)
    size_t size_bytes;          // Expected file size
} UpdateInfo;

/**
 * Initialize update system
 */
void update_init(void);

/**
 * Cleanup update resources
 */
void update_cleanup(void);

/**
 * Start async check for updates via GitHub API
 * Sets state to UPDATE_CHECKING, then UPDATE_AVAILABLE or UPDATE_UP_TO_DATE
 */
void update_check(void);

/**
 * Update the check (call each frame while UPDATE_CHECKING)
 * @return true when check is complete (success or error)
 */
bool update_check_complete(void);

/**
 * Start downloading the update binary
 * Sets state to UPDATE_DOWNLOADING
 */
void update_download(void);

/**
 * Update the download (call each frame while UPDATE_DOWNLOADING)
 * @return true when download is complete (success or error)
 */
bool update_download_complete(void);

/**
 * Apply the downloaded update
 * Backs up current binary, replaces with new, sets executable
 * Sets state to UPDATE_READY on success, UPDATE_ERROR on failure
 */
void update_apply(void);

/**
 * Get current update state
 */
UpdateState update_get_state(void);

/**
 * Get update info (only valid when state >= UPDATE_AVAILABLE)
 */
const UpdateInfo* update_get_info(void);

/**
 * Get error message (only valid when state == UPDATE_ERROR)
 */
const char* update_get_error(void);

/**
 * Get download progress (0-100)
 * Only valid when state == UPDATE_DOWNLOADING
 */
int update_get_progress(void);

/**
 * Reset state to IDLE (for retry or dismissal)
 */
void update_reset(void);

#endif // UPDATE_H
