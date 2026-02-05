/**
 * YouTube Music Integration via yt-dlp
 *
 * Provides YouTube search and download functionality using yt-dlp CLI.
 * Downloads audio to temp files which are then played with existing audio.c.
 */

#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <stdbool.h>

// Maximum search results
#define YOUTUBE_MAX_RESULTS 10

// Result entry from YouTube search
typedef struct {
    char id[16];           // Video ID (e.g., "dQw4w9WgXcQ")
    char title[256];       // Video title
    char channel[128];     // Channel name
    int duration_sec;      // Duration in seconds
} YouTubeResult;

// Download progress callback
// Returns false to cancel download
typedef bool (*YouTubeProgressCallback)(int percent, const char *status);

/**
 * Initialize YouTube system
 * Checks for yt-dlp availability
 */
void youtube_init(void);

/**
 * Cleanup YouTube resources
 * Removes temp files
 */
void youtube_cleanup(void);

/**
 * Check if YouTube functionality is available
 * @return true if yt-dlp is found and executable
 */
bool youtube_is_available(void);

/**
 * Search YouTube for music
 * @param query Search query string
 * @param results Array to fill with results
 * @param max_results Maximum results to return (up to YOUTUBE_MAX_RESULTS)
 * @return Number of results found, or -1 on error
 */
int youtube_search(const char *query, YouTubeResult *results, int max_results);

/**
 * Download audio from YouTube video
 * @param video_id YouTube video ID
 * @param title Video title (used for filename, can be NULL for fallback to video_id)
 * @param progress_cb Optional progress callback (can be NULL)
 * @return Path to downloaded temp file, or NULL on error
 */
const char* youtube_download(const char *video_id, const char *title, YouTubeProgressCallback progress_cb);

/**
 * Get the path to the last downloaded file
 * @return Path to temp file, or NULL if no download
 */
const char* youtube_get_temp_path(void);

/**
 * Get last error message
 * @return Error message string, or NULL if no error
 */
const char* youtube_get_error(void);

/**
 * Format duration in seconds to MM:SS string
 * @param duration_sec Duration in seconds
 * @param buffer Output buffer (at least 16 chars)
 */
void youtube_format_duration(int duration_sec, char *buffer);

/**
 * Get the download directory path
 * @return Path to download directory (e.g., "/mnt/SDCARD/Music/YouTube")
 */
const char* youtube_get_download_dir(void);

#endif // YOUTUBE_H
