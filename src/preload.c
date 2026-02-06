/**
 * Audio Preloader Implementation
 *
 * Background thread decodes next track while current plays.
 * Follows same threading pattern as download_queue.c.
 */

#include "preload.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// dr_flac for FLAC decoding (same as audio.c)
#include "dr_flac.h"

// Preload state
typedef enum {
    PRELOAD_IDLE,
    PRELOAD_LOADING,
    PRELOAD_READY,
    PRELOAD_CANCELLED
} PreloadState;

// Thread synchronization
static pthread_t g_worker_thread;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static bool g_worker_running = false;
static bool g_shutdown = false;

// Current preload state
static PreloadState g_state = PRELOAD_IDLE;
static char g_request_path[512] = {0};
static PreloadedTrack *g_ready_track = NULL;

/**
 * Check if file has FLAC extension
 */
static bool is_flac_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".flac") == 0);
}

/**
 * Decode FLAC to WAV in memory (full file, not chunked)
 * For preloading, we decode the entire file since we have time
 */
static PreloadedTrack* decode_flac_full(const char *path) {
    drflac *pFlac = drflac_open_file(path, NULL);
    if (!pFlac) {
        fprintf(stderr, "[PRELOAD] Failed to open FLAC: %s\n", path);
        return NULL;
    }

    uint32_t sample_rate = pFlac->sampleRate;
    uint32_t channels = pFlac->channels;
    uint64_t total_frames = pFlac->totalPCMFrameCount;
    int duration_sec = (int)(total_frames / sample_rate);

    // Allocate PCM buffer
    size_t pcm_size = total_frames * channels * sizeof(int16_t);
    int16_t *pcm_data = (int16_t *)malloc(pcm_size);
    if (!pcm_data) {
        fprintf(stderr, "[PRELOAD] Failed to allocate PCM buffer (%zu bytes)\n", pcm_size);
        drflac_close(pFlac);
        return NULL;
    }

    // Decode entire file
    uint64_t frames_read = drflac_read_pcm_frames_s16(pFlac, total_frames, pcm_data);
    drflac_close(pFlac);

    if (frames_read == 0) {
        free(pcm_data);
        return NULL;
    }

    // Build WAV in memory
    size_t wav_size = 44 + (frames_read * channels * sizeof(int16_t));
    uint8_t *wav_data = (uint8_t *)malloc(wav_size);
    if (!wav_data) {
        free(pcm_data);
        return NULL;
    }

    // WAV header
    uint32_t data_size = (uint32_t)(frames_read * channels * sizeof(int16_t));
    uint32_t file_size = 36 + data_size;
    uint16_t block_align = channels * sizeof(int16_t);
    uint32_t byte_rate = sample_rate * block_align;

    memcpy(wav_data + 0, "RIFF", 4);
    memcpy(wav_data + 4, &file_size, 4);
    memcpy(wav_data + 8, "WAVE", 4);
    memcpy(wav_data + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    memcpy(wav_data + 16, &fmt_size, 4);
    uint16_t audio_format = 1;
    memcpy(wav_data + 20, &audio_format, 2);
    uint16_t num_channels = (uint16_t)channels;
    memcpy(wav_data + 22, &num_channels, 2);
    memcpy(wav_data + 24, &sample_rate, 4);
    memcpy(wav_data + 28, &byte_rate, 4);
    memcpy(wav_data + 32, &block_align, 2);
    uint16_t bits_per_sample = 16;
    memcpy(wav_data + 34, &bits_per_sample, 2);
    memcpy(wav_data + 36, "data", 4);
    memcpy(wav_data + 40, &data_size, 4);
    memcpy(wav_data + 44, pcm_data, data_size);

    free(pcm_data);

    // Create result
    PreloadedTrack *track = (PreloadedTrack *)malloc(sizeof(PreloadedTrack));
    if (!track) {
        free(wav_data);
        return NULL;
    }

    strncpy(track->path, path, sizeof(track->path) - 1);
    track->path[sizeof(track->path) - 1] = '\0';
    track->wav_data = wav_data;
    track->wav_size = wav_size;
    track->sample_rate = (int)sample_rate;
    track->channels = (int)channels;
    track->duration_sec = duration_sec;
    track->is_flac = true;

    return track;
}

/**
 * Worker thread function
 */
static void* worker_func(void *arg) {
    (void)arg;
    printf("[PRELOAD] Worker thread started\n");

    while (1) {
        pthread_mutex_lock(&g_mutex);

        // Wait for work or shutdown
        while (g_state != PRELOAD_LOADING && !g_shutdown) {
            pthread_cond_wait(&g_cond, &g_mutex);
        }

        if (g_shutdown) {
            pthread_mutex_unlock(&g_mutex);
            break;
        }

        // Copy path to local variable
        char path[512];
        strncpy(path, g_request_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';

        pthread_mutex_unlock(&g_mutex);

        printf("[PRELOAD] Starting preload: %s\n", path);

        // Decode the file
        PreloadedTrack *track = NULL;

        if (is_flac_file(path)) {
            track = decode_flac_full(path);
        } else {
            // For MP3/OGG, create a minimal track struct
            // SDL_mixer will handle loading, we just mark it as ready
            track = (PreloadedTrack *)malloc(sizeof(PreloadedTrack));
            if (track) {
                memset(track, 0, sizeof(PreloadedTrack));
                strncpy(track->path, path, sizeof(track->path) - 1);
                track->is_flac = false;
                // wav_data = NULL means use native SDL_mixer loading
            }
        }

        // Check if cancelled during decode
        pthread_mutex_lock(&g_mutex);

        if (g_state == PRELOAD_CANCELLED || g_shutdown) {
            // Cancelled - discard result
            if (track) {
                if (track->wav_data) free(track->wav_data);
                free(track);
            }
            g_state = PRELOAD_IDLE;
            printf("[PRELOAD] Cancelled: %s\n", path);
        } else if (track) {
            // Success
            if (g_ready_track) {
                if (g_ready_track->wav_data) free(g_ready_track->wav_data);
                free(g_ready_track);
            }
            g_ready_track = track;
            g_state = PRELOAD_READY;
            printf("[PRELOAD] Ready: %s (FLAC=%d, %d sec)\n",
                   path, track->is_flac, track->duration_sec);
        } else {
            // Failed
            g_state = PRELOAD_IDLE;
            printf("[PRELOAD] Failed: %s\n", path);
        }

        pthread_mutex_unlock(&g_mutex);
    }

    printf("[PRELOAD] Worker thread exiting\n");
    return NULL;
}

void preload_init(void) {
    pthread_mutex_lock(&g_mutex);

    g_state = PRELOAD_IDLE;
    g_request_path[0] = '\0';
    g_ready_track = NULL;
    g_shutdown = false;

    pthread_mutex_unlock(&g_mutex);

    // Start worker thread
    if (!g_worker_running) {
        if (pthread_create(&g_worker_thread, NULL, worker_func, NULL) == 0) {
            g_worker_running = true;
            printf("[PRELOAD] Initialized\n");
        } else {
            fprintf(stderr, "[PRELOAD] Failed to create worker thread\n");
        }
    }
}

void preload_cleanup(void) {
    if (!g_worker_running) return;

    printf("[PRELOAD] Shutting down...\n");

    pthread_mutex_lock(&g_mutex);
    g_shutdown = true;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);

    pthread_join(g_worker_thread, NULL);
    g_worker_running = false;

    // Free any remaining track
    if (g_ready_track) {
        if (g_ready_track->wav_data) free(g_ready_track->wav_data);
        free(g_ready_track);
        g_ready_track = NULL;
    }

    printf("[PRELOAD] Shutdown complete\n");
}

void preload_start(const char *path) {
    if (!path || !g_worker_running) return;

    pthread_mutex_lock(&g_mutex);

    // Cancel any current operation
    if (g_state == PRELOAD_LOADING) {
        g_state = PRELOAD_CANCELLED;
    }

    // Free any ready track that wasn't consumed
    if (g_ready_track) {
        if (g_ready_track->wav_data) free(g_ready_track->wav_data);
        free(g_ready_track);
        g_ready_track = NULL;
    }

    // Start new preload
    strncpy(g_request_path, path, sizeof(g_request_path) - 1);
    g_request_path[sizeof(g_request_path) - 1] = '\0';
    g_state = PRELOAD_LOADING;

    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);

    printf("[PRELOAD] Requested: %s\n", path);
}

void preload_cancel(void) {
    pthread_mutex_lock(&g_mutex);

    if (g_state == PRELOAD_LOADING) {
        g_state = PRELOAD_CANCELLED;
    }

    if (g_ready_track) {
        if (g_ready_track->wav_data) free(g_ready_track->wav_data);
        free(g_ready_track);
        g_ready_track = NULL;
    }

    g_state = PRELOAD_IDLE;

    pthread_mutex_unlock(&g_mutex);
}

bool preload_is_ready(void) {
    pthread_mutex_lock(&g_mutex);
    bool ready = (g_state == PRELOAD_READY && g_ready_track != NULL);
    pthread_mutex_unlock(&g_mutex);
    return ready;
}

const char* preload_get_path(void) {
    // Note: not thread-safe for reading, but path is static
    if (g_state == PRELOAD_IDLE) return NULL;
    return g_request_path;
}

PreloadedTrack* preload_consume(const char *path) {
    if (!path) return NULL;

    pthread_mutex_lock(&g_mutex);

    PreloadedTrack *result = NULL;

    if (g_state == PRELOAD_READY && g_ready_track != NULL) {
        // Validate path matches
        if (strcmp(g_ready_track->path, path) == 0) {
            result = g_ready_track;
            g_ready_track = NULL;
            g_state = PRELOAD_IDLE;
            printf("[PRELOAD] Consumed: %s\n", path);
        } else {
            printf("[PRELOAD] Path mismatch: expected %s, got %s\n",
                   path, g_ready_track->path);
        }
    }

    pthread_mutex_unlock(&g_mutex);
    return result;
}

void preload_free_track(PreloadedTrack *track) {
    if (!track) return;
    if (track->wav_data) free(track->wav_data);
    free(track);
}
