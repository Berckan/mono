/**
 * Audio Preloader - Background decoding for gapless playback
 *
 * Pre-decodes the next track in a background thread while the current
 * track plays. When track transition occurs, the pre-decoded audio
 * is immediately available, eliminating the 200-600ms gap.
 */

#ifndef PRELOAD_H
#define PRELOAD_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Preloaded track data (decoded audio ready to play)
 */
typedef struct {
    char path[512];           // Original file path
    uint8_t *wav_data;        // WAV data in memory (for FLAC)
    size_t wav_size;          // Size of WAV data
    int sample_rate;          // Sample rate
    int channels;             // Number of channels
    int duration_sec;         // Total duration in seconds
    bool is_flac;             // True if decoded from FLAC
} PreloadedTrack;

/**
 * Initialize preloader (call once at startup)
 */
void preload_init(void);

/**
 * Shutdown preloader (call at cleanup)
 */
void preload_cleanup(void);

/**
 * Start preloading a track in the background
 * Cancels any current preload operation
 * @param path Path to audio file to preload
 */
void preload_start(const char *path);

/**
 * Cancel current preload operation
 */
void preload_cancel(void);

/**
 * Check if preloaded track is ready
 * @return true if a track has been fully preloaded
 */
bool preload_is_ready(void);

/**
 * Get the path of the currently preloaded/preloading track
 * @return Path string, or NULL if nothing is preloading
 */
const char* preload_get_path(void);

/**
 * Consume the preloaded track (takes ownership)
 * Caller is responsible for freeing with preload_free_track()
 * @param path Expected path (for validation)
 * @return Preloaded track data, or NULL if not ready/mismatch
 */
PreloadedTrack* preload_consume(const char *path);

/**
 * Free a preloaded track's resources
 * @param track Track to free
 */
void preload_free_track(PreloadedTrack *track);

#endif // PRELOAD_H
