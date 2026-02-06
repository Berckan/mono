/**
 * Favorites System - Track favorite tracks
 *
 * Allows users to mark tracks as favorites and persist the list.
 * Favorites are stored as a JSON array of file paths.
 *
 * Path: ~/.userdata/tg5040/Mono/favorites.json
 */

#ifndef FAVORITES_H
#define FAVORITES_H

#include <stdbool.h>

// Maximum number of favorites
#define MAX_FAVORITES 256

/**
 * Initialize favorites system
 * Loads existing favorites from disk
 * @return 0 on success, -1 on failure
 */
int favorites_init(void);

/**
 * Cleanup and save favorites
 */
void favorites_cleanup(void);

/**
 * Add a track to favorites
 * @param path Full path to the audio file
 * @return true if added successfully
 */
bool favorites_add(const char *path);

/**
 * Remove a track from favorites
 * @param path Full path to the audio file
 * @return true if removed successfully
 */
bool favorites_remove(const char *path);

/**
 * Toggle favorite status for a track
 * @param path Full path to the audio file
 * @return true if track is now a favorite, false if removed
 */
bool favorites_toggle(const char *path);

/**
 * Check if a track is a favorite
 * @param path Full path to the audio file
 * @return true if track is a favorite
 */
bool favorites_is_favorite(const char *path);

/**
 * Get number of favorites
 * @return Number of favorite tracks
 */
int favorites_get_count(void);

/**
 * Get favorite path at index
 * @param index Index in favorites list
 * @return Path string, NULL if invalid index
 */
const char* favorites_get_path(int index);

/**
 * Save favorites to disk
 * @return true if saved successfully
 */
bool favorites_save(void);

// ========================================
// Favorites Playback Mode
// ========================================

/**
 * Set favorites playback mode
 * When enabled, NEXT/PREV and auto-advance use favorites list instead of browser
 * @param enabled Whether to enable favorites playback mode
 * @param start_index Index in favorites list to start from
 */
void favorites_set_playback_mode(bool enabled, int start_index);

/**
 * Check if favorites playback mode is active
 * @return true if playing from favorites
 */
bool favorites_is_playback_mode(void);

/**
 * Advance to next/prev favorite
 * @param delta +1 for next, -1 for previous
 * @return New index, or -1 if at boundary
 */
int favorites_advance_playback(int delta);

/**
 * Get path of current playback favorite
 * @return Path string, or NULL if not in playback mode
 */
const char* favorites_get_current_playback_path(void);

/**
 * Get current playback index
 * @return Current index in favorites list
 */
int favorites_get_playback_index(void);

/**
 * Set playback index directly (for shuffle)
 * @param index New index to set
 */
void favorites_set_playback_index(int index);

#endif // FAVORITES_H
