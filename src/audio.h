/**
 * Audio Engine - SDL_mixer based music playback
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

/**
 * Track metadata from ID3 tags or filename
 */
typedef struct {
    char title[256];
    char artist[256];
    char album[256];
    int duration_sec;      // Total duration in seconds
    int position_sec;      // Current position in seconds
} TrackInfo;

/**
 * Initialize audio engine
 * @return 0 on success, -1 on failure
 */
int audio_init(void);

/**
 * Cleanup audio resources
 */
void audio_cleanup(void);

/**
 * Load an audio file for playback
 * @param path Path to the audio file
 * @return true if loaded successfully
 */
bool audio_load(const char *path);

/**
 * Start or resume playback
 */
void audio_play(void);

/**
 * Pause playback
 */
void audio_pause(void);

/**
 * Toggle play/pause state
 */
void audio_toggle_pause(void);

/**
 * Stop playback and unload current track
 */
void audio_stop(void);

/**
 * Check if audio is currently playing
 * @return true if playing
 */
bool audio_is_playing(void);

/**
 * Check if audio is paused
 * @return true if paused
 */
bool audio_is_paused(void);

/**
 * Seek relative to current position
 * @param seconds Seconds to seek (negative for backward)
 */
void audio_seek(int seconds);

/**
 * Set volume level
 * @param volume Volume from 0 to 100
 */
void audio_set_volume(int volume);

/**
 * Get current volume
 * @return Volume from 0 to 100
 */
int audio_get_volume(void);

/**
 * Update audio state (call every frame)
 */
void audio_update(void);

/**
 * Get current track information
 * @return Pointer to track info (valid until next load)
 */
const TrackInfo* audio_get_track_info(void);

#endif // AUDIO_H
