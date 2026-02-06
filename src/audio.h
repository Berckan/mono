/**
 * Audio Engine - SDL_mixer based music playback
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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
 * Seek to absolute position
 * @param position_sec Position in seconds from start
 */
void audio_seek_absolute(int position_sec);

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
 * Enable/disable Bluetooth audio mode
 * When enabled, volume changes also sync with bluealsa
 * @param enabled true to enable BT mode
 */
void audio_set_bluetooth_mode(bool enabled);

/**
 * Update audio state (call every frame)
 */
void audio_update(void);

/**
 * Get current track information
 * @return Pointer to track info (valid until next load)
 */
const TrackInfo* audio_get_track_info(void);

/**
 * Get PCM data for waveform visualization (deprecated - always returns NULL)
 * @param sample_count Output: number of samples per channel
 * @param channels Output: number of channels
 * @param sample_rate Output: sample rate in Hz
 * @return Always NULL (waveform feature removed)
 */
const int16_t* audio_get_pcm_data(size_t *sample_count, int *channels, int *sample_rate);

/**
 * Check if PCM data is available (deprecated - always returns false)
 * @return Always false (waveform feature removed)
 */
bool audio_has_pcm_data(void);

/**
 * Get uppercase format string for currently playing track
 * @return "MP3", "FLAC", "OGG", "WAV", "M4A", "WEBM", "OPUS", or ""
 */
const char* audio_get_format_string(void);

/**
 * Get uppercase format string from any file path (extension-based)
 * @param path File path to check
 * @return "MP3", "FLAC", "OGG", "WAV", "M4A", "WEBM", "OPUS", or ""
 */
const char* audio_format_from_path(const char *path);

/**
 * Check if currently loaded track is a FLAC file
 * @return true if current track is FLAC (uses chunked decoding)
 */
bool audio_is_flac(void);

/**
 * Load audio from preloaded WAV data (for gapless playback)
 * Takes ownership of the wav_data pointer
 * @param path Original file path (for metadata)
 * @param wav_data WAV data in memory (will be freed by audio_stop)
 * @param wav_size Size of WAV data
 * @param duration_sec Total duration in seconds
 * @return true if loaded successfully
 */
bool audio_load_preloaded(const char *path, uint8_t *wav_data, size_t wav_size, int duration_sec);

#endif // AUDIO_H
