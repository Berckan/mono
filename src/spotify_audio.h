/**
 * Spotify Audio Pipe Reader
 *
 * Background pthread reads raw PCM from librespot's named FIFO pipe,
 * wraps chunks with WAV headers, and feeds them to SDL_mixer via
 * SDL_RWFromMem() (same pattern as FLAC chunked decode in audio.c).
 *
 * Audio format: S16LE, 44100Hz, stereo (librespot --format S16 --bitrate 160)
 * Buffer: ~5 seconds of pre-buffer before first playback
 */

#ifndef SPOTIFY_AUDIO_H
#define SPOTIFY_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// PCM format constants (matching librespot --format S16)
#define SP_SAMPLE_RATE  44100
#define SP_CHANNELS     2
#define SP_BITS         16
#define SP_BYTES_PER_SAMPLE (SP_BITS / 8 * SP_CHANNELS)  // 4 bytes per frame
#define SP_BYTES_PER_SEC    (SP_SAMPLE_RATE * SP_BYTES_PER_SAMPLE)  // ~176KB/s

// Buffer sizes
#define SP_PREBUFFER_SECONDS  3   // Pre-buffer before starting playback
#define SP_BUFFER_SECONDS     30  // Total ring buffer capacity (~5MB)

/**
 * Initialize the Spotify audio pipe reader
 * @param fifo_path Path to the named FIFO pipe
 * @return true if initialized successfully
 */
bool sp_audio_init(const char *fifo_path);

/**
 * Shutdown the pipe reader and free resources
 */
void sp_audio_cleanup(void);

/**
 * Start reading from the FIFO pipe (launches background thread)
 * @return true if thread started
 */
bool sp_audio_start(void);

/**
 * Stop reading (kills thread, does not close pipe)
 */
void sp_audio_stop(void);

/**
 * Check if enough data has been buffered for playback
 * @return true if pre-buffer threshold reached
 */
bool sp_audio_is_ready(void);

/**
 * Check if the pipe reader is actively receiving data
 * @return true if data has been received recently
 */
bool sp_audio_is_receiving(void);

/**
 * Check if pipe hit EOF (librespot stopped/track ended)
 * @return true if EOF detected
 */
bool sp_audio_is_eof(void);

/**
 * Get a WAV-wrapped chunk of audio for SDL_mixer
 * Caller must free the returned buffer when done.
 * @param out_size Output: size of WAV data
 * @return WAV data buffer (caller frees), or NULL if not enough data
 */
uint8_t* sp_audio_get_wav_chunk(size_t *out_size);

/**
 * Get how many seconds of audio are buffered
 * @return Buffered seconds (approximate)
 */
int sp_audio_buffered_seconds(void);

/**
 * Get total bytes read from pipe
 */
size_t sp_audio_bytes_read(void);

/**
 * Reset buffer state (for track transitions)
 */
void sp_audio_reset(void);

#endif // SPOTIFY_AUDIO_H
