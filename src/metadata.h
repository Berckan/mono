/**
 * Metadata Scanner - MusicBrainz API Integration
 *
 * Provides automatic metadata lookup via MusicBrainz API using curl CLI.
 * Results are cached locally to avoid repeated lookups.
 */

#ifndef METADATA_H
#define METADATA_H

#include <stdbool.h>

// Cached metadata entry
typedef struct {
    char title[256];
    char artist[256];
    char album[256];
    int confidence;  // 0-100, how confident is the match
} MetadataResult;

// Scan progress callback
// Returns false to cancel scan
typedef bool (*ScanProgressCallback)(int current, int total, const char *current_file);

/**
 * Initialize metadata system, load cache from disk
 */
void metadata_init(void);

/**
 * Cleanup and save cache to disk
 */
void metadata_cleanup(void);

/**
 * Lookup metadata for a single file via MusicBrainz
 * @param filepath Path to audio file
 * @param result Output result
 * @return true if found, false if not found or error
 */
bool metadata_lookup(const char *filepath, MetadataResult *result);

/**
 * Scan all audio files in a folder for metadata
 * @param folder_path Path to folder to scan
 * @param progress_cb Callback for progress updates (can be NULL)
 * @return Number of files successfully matched
 */
int metadata_scan_folder(const char *folder_path, ScanProgressCallback progress_cb);

/**
 * Get cached metadata for a file (if available)
 * @param filepath Path to audio file
 * @param result Output result
 * @return true if cached, false if not in cache
 */
bool metadata_get_cached(const char *filepath, MetadataResult *result);

/**
 * Check if file has cached metadata
 */
bool metadata_has_cache(const char *filepath);

/**
 * Clear all cached metadata
 */
void metadata_clear_cache(void);

/**
 * Create backup of current cache before scanning
 * @return true if backup created successfully
 */
bool metadata_backup_cache(void);

/**
 * Restore cache from backup (undo last scan)
 * @return true if restored successfully
 */
bool metadata_restore_backup(void);

/**
 * Check if backup exists
 */
bool metadata_has_backup(void);

/**
 * Get cache statistics
 */
void metadata_get_stats(int *total_cached, int *total_lookups);

#endif // METADATA_H
