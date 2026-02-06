/**
 * Spotify Audio Pipe Reader Implementation
 *
 * Background pthread reads raw S16LE PCM from librespot's named FIFO,
 * stores in a ring buffer, and provides WAV-wrapped chunks to SDL_mixer.
 *
 * Threading model follows preload.c: pthread with mutex/cond sync.
 * WAV assembly follows audio.c load_flac_from_position(): SDL_RWFromMem().
 */

#include "spotify_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// Ring buffer for PCM data
typedef struct {
    uint8_t *data;
    size_t capacity;     // Total buffer size in bytes
    size_t write_pos;    // Next write position
    size_t read_pos;     // Next read position
    size_t available;    // Bytes available to read
    pthread_mutex_t mutex;
} RingBuffer;

// WAV header (44 bytes) for wrapping raw PCM
#pragma pack(push, 1)
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // 16
    uint16_t audio_format;  // 1 (PCM)
    uint16_t channels;      // 2
    uint32_t sample_rate;   // 44100
    uint32_t byte_rate;     // sample_rate * channels * bits/8
    uint16_t block_align;   // channels * bits/8
    uint16_t bits_per_sample; // 16
    char data_id[4];        // "data"
    uint32_t data_size;     // PCM data size
} WavHeader;
#pragma pack(pop)

// State
static char g_fifo_path[256] = {0};
static int g_fifo_fd = -1;
static bool g_running = false;
static bool g_eof = false;
static bool g_receiving = false;
static pthread_t g_thread;
static RingBuffer g_buffer;
static size_t g_total_bytes_read = 0;

// Timestamp of last data received
static uint32_t g_last_data_time = 0;

/**
 * Initialize ring buffer
 */
static bool ringbuf_init(RingBuffer *rb, size_t capacity) {
    rb->data = malloc(capacity);
    if (!rb->data) return false;
    rb->capacity = capacity;
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->available = 0;
    pthread_mutex_init(&rb->mutex, NULL);
    return true;
}

/**
 * Free ring buffer
 */
static void ringbuf_free(RingBuffer *rb) {
    if (rb->data) {
        free(rb->data);
        rb->data = NULL;
    }
    pthread_mutex_destroy(&rb->mutex);
}

/**
 * Write data to ring buffer (called from reader thread)
 * @return Number of bytes actually written
 */
static size_t ringbuf_write(RingBuffer *rb, const uint8_t *data, size_t len) {
    pthread_mutex_lock(&rb->mutex);

    size_t space = rb->capacity - rb->available;
    if (len > space) len = space;  // Drop excess if buffer full

    // Write in up to two chunks (wrap around)
    size_t first_chunk = rb->capacity - rb->write_pos;
    if (first_chunk > len) first_chunk = len;

    memcpy(rb->data + rb->write_pos, data, first_chunk);
    if (len > first_chunk) {
        memcpy(rb->data, data + first_chunk, len - first_chunk);
    }

    rb->write_pos = (rb->write_pos + len) % rb->capacity;
    rb->available += len;

    pthread_mutex_unlock(&rb->mutex);
    return len;
}

/**
 * Read data from ring buffer (called from main thread)
 * @return Number of bytes actually read
 */
static size_t ringbuf_read(RingBuffer *rb, uint8_t *out, size_t len) {
    pthread_mutex_lock(&rb->mutex);

    if (len > rb->available) len = rb->available;

    // Read in up to two chunks (wrap around)
    size_t first_chunk = rb->capacity - rb->read_pos;
    if (first_chunk > len) first_chunk = len;

    memcpy(out, rb->data + rb->read_pos, first_chunk);
    if (len > first_chunk) {
        memcpy(out + first_chunk, rb->data, len - first_chunk);
    }

    rb->read_pos = (rb->read_pos + len) % rb->capacity;
    rb->available -= len;

    pthread_mutex_unlock(&rb->mutex);
    return len;
}

/**
 * Get available bytes in ring buffer
 */
static size_t ringbuf_available(RingBuffer *rb) {
    pthread_mutex_lock(&rb->mutex);
    size_t avail = rb->available;
    pthread_mutex_unlock(&rb->mutex);
    return avail;
}

/**
 * Reset ring buffer (clear all data)
 */
static void ringbuf_reset(RingBuffer *rb) {
    pthread_mutex_lock(&rb->mutex);
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->available = 0;
    pthread_mutex_unlock(&rb->mutex);
}

/**
 * Build a WAV header for the given PCM data size
 */
static void build_wav_header(WavHeader *hdr, uint32_t pcm_size) {
    memcpy(hdr->riff, "RIFF", 4);
    hdr->file_size = 36 + pcm_size;
    memcpy(hdr->wave, "WAVE", 4);
    memcpy(hdr->fmt, "fmt ", 4);
    hdr->fmt_size = 16;
    hdr->audio_format = 1;  // PCM
    hdr->channels = SP_CHANNELS;
    hdr->sample_rate = SP_SAMPLE_RATE;
    hdr->byte_rate = SP_BYTES_PER_SEC;
    hdr->block_align = SP_BYTES_PER_SAMPLE;
    hdr->bits_per_sample = SP_BITS;
    memcpy(hdr->data_id, "data", 4);
    hdr->data_size = pcm_size;
}

/**
 * Background thread: reads PCM data from FIFO pipe into ring buffer
 */
static void* pipe_reader_thread(void *arg) {
    (void)arg;

    printf("[SP_AUDIO] Reader thread started\n");

    // Open FIFO in non-blocking mode first, then switch to blocking
    // This avoids blocking forever if librespot hasn't opened its end yet
    g_fifo_fd = open(g_fifo_path, O_RDONLY | O_NONBLOCK);
    if (g_fifo_fd < 0) {
        fprintf(stderr, "[SP_AUDIO] Failed to open FIFO: %s\n", strerror(errno));
        g_eof = true;
        return NULL;
    }

    // Switch to blocking mode for efficient reads
    int flags = fcntl(g_fifo_fd, F_GETFL);
    fcntl(g_fifo_fd, F_SETFL, flags & ~O_NONBLOCK);

    uint8_t read_buf[8192];  // 8KB read chunks (~46ms of audio)

    while (g_running) {
        ssize_t bytes = read(g_fifo_fd, read_buf, sizeof(read_buf));

        if (bytes > 0) {
            ringbuf_write(&g_buffer, read_buf, bytes);
            g_total_bytes_read += bytes;
            g_receiving = true;
            g_last_data_time = SDL_GetTicks();
        } else if (bytes == 0) {
            // EOF - librespot closed the pipe (track ended or process stopped)
            printf("[SP_AUDIO] Pipe EOF (track ended)\n");
            g_eof = true;
            g_receiving = false;
            break;
        } else {
            // Error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available (shouldn't happen in blocking mode)
                usleep(10000);  // 10ms
                continue;
            }
            if (errno == EINTR) continue;  // Interrupted by signal

            fprintf(stderr, "[SP_AUDIO] Read error: %s\n", strerror(errno));
            g_eof = true;
            g_receiving = false;
            break;
        }
    }

    if (g_fifo_fd >= 0) {
        close(g_fifo_fd);
        g_fifo_fd = -1;
    }

    printf("[SP_AUDIO] Reader thread exiting (total: %zu bytes)\n", g_total_bytes_read);
    return NULL;
}

bool sp_audio_init(const char *fifo_path) {
    if (!fifo_path) return false;

    strncpy(g_fifo_path, fifo_path, sizeof(g_fifo_path) - 1);
    g_running = false;
    g_eof = false;
    g_receiving = false;
    g_fifo_fd = -1;
    g_total_bytes_read = 0;
    g_last_data_time = 0;

    // Allocate ring buffer (~5MB for 30 seconds of stereo S16)
    size_t buf_size = SP_BUFFER_SECONDS * SP_BYTES_PER_SEC;
    if (!ringbuf_init(&g_buffer, buf_size)) {
        fprintf(stderr, "[SP_AUDIO] Failed to allocate %zu byte buffer\n", buf_size);
        return false;
    }

    printf("[SP_AUDIO] Initialized (buffer=%zu bytes, %d sec)\n", buf_size, SP_BUFFER_SECONDS);
    return true;
}

void sp_audio_cleanup(void) {
    sp_audio_stop();
    ringbuf_free(&g_buffer);
    printf("[SP_AUDIO] Cleanup complete\n");
}

bool sp_audio_start(void) {
    if (g_running) return true;  // Already running

    g_running = true;
    g_eof = false;
    g_receiving = false;
    g_total_bytes_read = 0;

    ringbuf_reset(&g_buffer);

    if (pthread_create(&g_thread, NULL, pipe_reader_thread, NULL) != 0) {
        fprintf(stderr, "[SP_AUDIO] Failed to create reader thread\n");
        g_running = false;
        return false;
    }

    printf("[SP_AUDIO] Reader thread launched\n");
    return true;
}

void sp_audio_stop(void) {
    if (!g_running) return;

    g_running = false;

    // Close FIFO to unblock the reader thread
    if (g_fifo_fd >= 0) {
        close(g_fifo_fd);
        g_fifo_fd = -1;
    }

    pthread_join(g_thread, NULL);
    printf("[SP_AUDIO] Reader thread stopped\n");
}

bool sp_audio_is_ready(void) {
    size_t threshold = SP_PREBUFFER_SECONDS * SP_BYTES_PER_SEC;
    return ringbuf_available(&g_buffer) >= threshold;
}

bool sp_audio_is_receiving(void) {
    if (!g_receiving) return false;

    // Consider "not receiving" if no data for 3 seconds
    uint32_t now = SDL_GetTicks();
    if (g_last_data_time > 0 && (now - g_last_data_time) > 3000) {
        return false;
    }
    return true;
}

bool sp_audio_is_eof(void) {
    return g_eof;
}

uint8_t* sp_audio_get_wav_chunk(size_t *out_size) {
    // Read up to 5 seconds of PCM at a time
    size_t max_pcm = 5 * SP_BYTES_PER_SEC;
    size_t available = ringbuf_available(&g_buffer);

    if (available == 0) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    size_t pcm_size = (available < max_pcm) ? available : max_pcm;

    // Align to sample boundary (4 bytes per frame for stereo S16)
    pcm_size = (pcm_size / SP_BYTES_PER_SAMPLE) * SP_BYTES_PER_SAMPLE;
    if (pcm_size == 0) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    // Allocate WAV buffer (header + PCM data)
    size_t wav_size = sizeof(WavHeader) + pcm_size;
    uint8_t *wav_buf = malloc(wav_size);
    if (!wav_buf) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    // Write WAV header
    WavHeader *hdr = (WavHeader *)wav_buf;
    build_wav_header(hdr, (uint32_t)pcm_size);

    // Read PCM data from ring buffer
    size_t actual = ringbuf_read(&g_buffer, wav_buf + sizeof(WavHeader), pcm_size);
    if (actual < pcm_size) {
        // Update header with actual size
        hdr->data_size = (uint32_t)actual;
        hdr->file_size = 36 + (uint32_t)actual;
        wav_size = sizeof(WavHeader) + actual;
    }

    if (out_size) *out_size = wav_size;
    return wav_buf;
}

int sp_audio_buffered_seconds(void) {
    size_t avail = ringbuf_available(&g_buffer);
    return (int)(avail / SP_BYTES_PER_SEC);
}

size_t sp_audio_bytes_read(void) {
    return g_total_bytes_read;
}

void sp_audio_reset(void) {
    ringbuf_reset(&g_buffer);
    g_eof = false;
    g_receiving = false;
    g_total_bytes_read = 0;
    g_last_data_time = 0;
}
