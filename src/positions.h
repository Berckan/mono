/**
 * Position Persistence - Remember playback position per file
 *
 * Stores playback positions for each file so users can resume
 * from where they left off, even after switching between files
 * or restarting the app.
 */

#ifndef POSITIONS_H
#define POSITIONS_H

#include <stdbool.h>

/**
 * Initialize positions system
 * Loads saved positions from disk
 * @return 0 on success, -1 on failure
 */
int positions_init(void);

/**
 * Save position for a file
 * @param path Full path to the audio file
 * @param position_sec Position in seconds
 */
void positions_set(const char *path, int position_sec);

/**
 * Get saved position for a file
 * @param path Full path to the audio file
 * @return Saved position in seconds, or 0 if not found
 */
int positions_get(const char *path);

/**
 * Clear position for a file (e.g., when playback completes)
 * @param path Full path to the audio file
 */
void positions_clear(const char *path);

/**
 * Save all positions to disk
 * Call this before exit or periodically
 */
void positions_save(void);

/**
 * Cleanup positions system
 * Saves positions and frees memory
 */
void positions_cleanup(void);

#endif // POSITIONS_H
