/**
 * Audio Engine Implementation
 *
 * Uses SDL_mixer for audio playback with support for MP3, FLAC, and OGG.
 * FLAC files are decoded via dr_flac since SDL_mixer on Trimui lacks FLAC support.
 * Provides basic ID3 tag extraction for metadata display.
 */

#include "audio.h"
#include "metadata.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>

// dr_flac - single-file FLAC decoder (SDL_mixer on Trimui lacks FLAC support)
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

// Current track
static Mix_Music *g_music = NULL;
static TrackInfo g_track_info;
static bool g_is_paused = false;
static int g_volume = 80;  // Default 80%

// Track start time for position calculation
static Uint32 g_start_time = 0;
static Uint32 g_pause_time = 0;
static double g_music_position = 0.0;

// FLAC decoded to WAV in memory
static uint8_t *g_flac_wav_data = NULL;
static size_t g_flac_wav_size = 0;
static int g_flac_sample_rate = 0;
static int g_flac_channels = 0;
static int g_flac_duration = 0;
static char g_current_path[512] = {0};  // For FLAC seek (reload from position)

/**
 * Check if file has FLAC extension
 */
static bool is_flac_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".flac") == 0);
}

/**
 * Decode FLAC to WAV in memory using dr_flac, starting from a given position
 * @param path Path to FLAC file
 * @param start_sec Starting position in seconds (0 = beginning)
 * @param out_data Output: pointer to allocated WAV data (caller must free)
 * @param out_size Output: size of WAV data
 * @param out_sample_rate Output: sample rate of decoded audio
 * @param out_channels Output: number of channels
 * @return Total duration in seconds, or 0 on failure
 */
static int decode_flac_to_wav(const char *path, int start_sec, uint8_t **out_data, size_t *out_size,
                               int *out_sample_rate, int *out_channels) {
    drflac *pFlac = drflac_open_file(path, NULL);
    if (!pFlac) {
        fprintf(stderr, "[AUDIO] Failed to open FLAC: %s\n", path);
        return 0;
    }

    // Get FLAC properties
    uint32_t sample_rate = pFlac->sampleRate;
    uint32_t channels = pFlac->channels;
    uint64_t total_frames = pFlac->totalPCMFrameCount;
    int duration_sec = (int)(total_frames / sample_rate);

    // Calculate start frame for seek
    uint64_t start_frame = 0;
    if (start_sec > 0 && start_sec < duration_sec) {
        start_frame = (uint64_t)start_sec * sample_rate;
        if (!drflac_seek_to_pcm_frame(pFlac, start_frame)) {
            fprintf(stderr, "[AUDIO] FLAC seek failed, starting from beginning\n");
            start_frame = 0;
            drflac_seek_to_pcm_frame(pFlac, 0);
        }
    }

    uint64_t frames_to_decode = total_frames - start_frame;

    // Allocate buffer for decoded PCM (16-bit samples)
    size_t pcm_size = frames_to_decode * channels * sizeof(int16_t);
    int16_t *pcm_data = (int16_t *)malloc(pcm_size);
    if (!pcm_data) {
        fprintf(stderr, "[AUDIO] Failed to allocate PCM buffer (%zu bytes)\n", pcm_size);
        drflac_close(pFlac);
        return 0;
    }

    // Decode frames
    uint64_t frames_read = drflac_read_pcm_frames_s16(pFlac, frames_to_decode, pcm_data);
    drflac_close(pFlac);

    // Build WAV file in memory (44-byte header + PCM data)
    size_t wav_size = 44 + (frames_read * channels * sizeof(int16_t));
    uint8_t *wav_data = (uint8_t *)malloc(wav_size);
    if (!wav_data) {
        fprintf(stderr, "[AUDIO] Failed to allocate WAV buffer\n");
        free(pcm_data);
        return 0;
    }

    // WAV header
    uint32_t data_size = (uint32_t)(frames_read * channels * sizeof(int16_t));
    uint32_t file_size = 36 + data_size;
    uint16_t block_align = channels * sizeof(int16_t);
    uint32_t byte_rate = sample_rate * block_align;

    // RIFF header
    memcpy(wav_data + 0, "RIFF", 4);
    memcpy(wav_data + 4, &file_size, 4);
    memcpy(wav_data + 8, "WAVE", 4);

    // fmt chunk
    memcpy(wav_data + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    memcpy(wav_data + 16, &fmt_size, 4);
    uint16_t audio_format = 1;  // PCM
    memcpy(wav_data + 20, &audio_format, 2);
    uint16_t num_channels = (uint16_t)channels;
    memcpy(wav_data + 22, &num_channels, 2);
    memcpy(wav_data + 24, &sample_rate, 4);
    memcpy(wav_data + 28, &byte_rate, 4);
    memcpy(wav_data + 32, &block_align, 2);
    uint16_t bits_per_sample = 16;
    memcpy(wav_data + 34, &bits_per_sample, 2);

    // data chunk
    memcpy(wav_data + 36, "data", 4);
    memcpy(wav_data + 40, &data_size, 4);

    // Copy PCM data
    memcpy(wav_data + 44, pcm_data, data_size);
    free(pcm_data);

    *out_data = wav_data;
    *out_size = wav_size;
    *out_sample_rate = (int)sample_rate;
    *out_channels = (int)channels;

    return duration_sec;
}

/**
 * Free FLAC WAV buffer
 */
static void free_flac_buffer(void) {
    if (g_flac_wav_data) {
        free(g_flac_wav_data);
        g_flac_wav_data = NULL;
    }
    g_flac_wav_size = 0;
    g_flac_sample_rate = 0;
    g_flac_channels = 0;
    g_flac_duration = 0;
}

/**
 * Load FLAC file from a given position
 */
static bool load_flac_from_position(const char *path, int start_sec) {
    // Decode FLAC to WAV starting from position
    int duration = decode_flac_to_wav(path, start_sec, &g_flac_wav_data, &g_flac_wav_size,
                                       &g_flac_sample_rate, &g_flac_channels);
    if (duration <= 0 || !g_flac_wav_data) {
        return false;
    }

    g_flac_duration = duration;

    // Load WAV from memory
    SDL_RWops *rwops = SDL_RWFromMem(g_flac_wav_data, (int)g_flac_wav_size);
    if (!rwops) {
        free_flac_buffer();
        return false;
    }

    // SDL_TRUE = SDL will free RWops when music is freed (but NOT our buffer)
    g_music = Mix_LoadMUS_RW(rwops, SDL_TRUE);
    if (!g_music) {
        free_flac_buffer();
        return false;
    }

    return true;
}

/**
 * Extract filename without extension as fallback title
 */
static void extract_filename_title(const char *path, char *title, size_t size) {
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++;
    } else {
        filename = path;
    }

    strncpy(title, filename, size - 1);
    title[size - 1] = '\0';

    char *dot = strrchr(title, '.');
    if (dot) {
        *dot = '\0';
    }
}

/**
 * Estimate MP3 duration from file size and bitrate
 */
static int estimate_mp3_duration(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Skip ID3v2 tag if present
    unsigned char header[10];
    if (fread(header, 1, 10, f) != 10) {
        fclose(f);
        return 0;
    }

    long audio_start = 0;
    if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        long tag_size = ((header[6] & 0x7F) << 21) |
                       ((header[7] & 0x7F) << 14) |
                       ((header[8] & 0x7F) << 7) |
                       (header[9] & 0x7F);
        audio_start = 10 + tag_size;
    }

    // Find first MP3 frame sync
    fseek(f, audio_start, SEEK_SET);
    unsigned char sync[2];
    int found = 0;
    for (int i = 0; i < 10000 && !found; i++) {
        if (fread(sync, 1, 2, f) != 2) break;
        if (sync[0] == 0xFF && (sync[1] & 0xE0) == 0xE0) {
            found = 1;
            fseek(f, -2, SEEK_CUR);
        } else {
            fseek(f, -1, SEEK_CUR);
        }
    }

    if (!found) {
        fclose(f);
        return 0;
    }

    unsigned char frame[4];
    if (fread(frame, 1, 4, f) != 4) {
        fclose(f);
        return 0;
    }
    fclose(f);

    int version = (frame[1] >> 3) & 0x03;
    int layer = (frame[1] >> 1) & 0x03;
    int bitrate_idx = (frame[2] >> 4) & 0x0F;

    static const int bitrates_v1_l3[] = {
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
    };
    static const int bitrates_v2_l3[] = {
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
    };

    int bitrate = 0;
    if (layer == 0x01) {
        if (version == 0x03) {
            bitrate = bitrates_v1_l3[bitrate_idx];
        } else if (version == 0x02 || version == 0x00) {
            bitrate = bitrates_v2_l3[bitrate_idx];
        }
    }

    if (bitrate == 0) return 0;

    long audio_size = file_size - audio_start - 128;
    if (audio_size < 0) audio_size = file_size - audio_start;

    return (int)((audio_size * 8) / (bitrate * 1000));
}

/**
 * Read ID3v2 tags from MP3 file (ID3v2.3/2.4)
 * Parses TIT2 (title), TPE1 (artist), TALB (album) frames
 */
static bool read_id3v2(const char *path, TrackInfo *info) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    // Read ID3v2 header (10 bytes)
    unsigned char header[10];
    if (fread(header, 1, 10, f) != 10) {
        fclose(f);
        return false;
    }

    // Check "ID3" magic
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
        fclose(f);
        return false;
    }

    // Version check (we handle 2.3 and 2.4)
    int version = header[3];
    if (version < 2 || version > 4) {
        fclose(f);
        return false;
    }

    // Tag size (syncsafe integer)
    uint32_t tag_size = ((header[6] & 0x7F) << 21) |
                        ((header[7] & 0x7F) << 14) |
                        ((header[8] & 0x7F) << 7) |
                        (header[9] & 0x7F);

    bool got_title = false, got_artist = false, got_album = false;
    long pos = 10;
    long end_pos = 10 + (long)tag_size;

    // Parse frames
    while (pos < end_pos && !(got_title && got_artist && got_album)) {
        // Read frame header (10 bytes for v2.3/2.4)
        unsigned char frame_header[10];
        if (fseek(f, pos, SEEK_SET) != 0) break;
        if (fread(frame_header, 1, 10, f) != 10) break;

        // Check for padding (null bytes = end of frames)
        if (frame_header[0] == 0) break;

        // Frame ID (4 chars)
        char frame_id[5];
        memcpy(frame_id, frame_header, 4);
        frame_id[4] = '\0';

        // Frame size
        uint32_t frame_size;
        if (version == 4) {
            // ID3v2.4: syncsafe integer
            frame_size = ((frame_header[4] & 0x7F) << 21) |
                         ((frame_header[5] & 0x7F) << 14) |
                         ((frame_header[6] & 0x7F) << 7) |
                         (frame_header[7] & 0x7F);
        } else {
            // ID3v2.3: big-endian 32-bit
            frame_size = ((uint32_t)frame_header[4] << 24) |
                         ((uint32_t)frame_header[5] << 16) |
                         ((uint32_t)frame_header[6] << 8) |
                         (uint32_t)frame_header[7];
        }

        // Sanity check
        if (frame_size == 0 || frame_size > 10000000) {
            pos += 10 + frame_size;
            continue;
        }

        // Check if this is a text frame we want
        char *dest = NULL;
        size_t dest_size = 0;

        if (strcmp(frame_id, "TIT2") == 0) {
            dest = info->title;
            dest_size = sizeof(info->title);
        } else if (strcmp(frame_id, "TPE1") == 0) {
            dest = info->artist;
            dest_size = sizeof(info->artist);
        } else if (strcmp(frame_id, "TALB") == 0) {
            dest = info->album;
            dest_size = sizeof(info->album);
        }

        if (dest && frame_size > 1) {
            // Read frame content
            unsigned char *content = malloc(frame_size);
            if (content) {
                if (fread(content, 1, frame_size, f) == frame_size) {
                    // First byte is encoding
                    int encoding = content[0];
                    unsigned char *text_start = content + 1;
                    uint32_t text_len = frame_size - 1;

                    // Handle encoding
                    if (encoding == 0 || encoding == 3) {
                        // ISO-8859-1 or UTF-8: direct copy
                        size_t copy_len = (text_len < dest_size - 1) ? text_len : dest_size - 1;
                        memcpy(dest, text_start, copy_len);
                        dest[copy_len] = '\0';
                    } else if (encoding == 1 || encoding == 2) {
                        // UTF-16 (with or without BOM): simple ASCII extraction
                        // (Full UTF-16 support would require iconv/ICU)
                        size_t j = 0;
                        int bom_skip = 0;
                        if (text_len >= 2 && ((text_start[0] == 0xFF && text_start[1] == 0xFE) ||
                                               (text_start[0] == 0xFE && text_start[1] == 0xFF))) {
                            bom_skip = 2;
                        }
                        bool is_be = (bom_skip == 2 && text_start[0] == 0xFE);

                        for (uint32_t i = bom_skip; i + 1 < text_len && j < dest_size - 1; i += 2) {
                            unsigned char lo = is_be ? text_start[i + 1] : text_start[i];
                            unsigned char hi = is_be ? text_start[i] : text_start[i + 1];
                            if (hi == 0 && lo >= 0x20 && lo < 0x7F) {
                                dest[j++] = (char)lo;
                            } else if (lo == 0 && hi == 0) {
                                break;  // Null terminator
                            }
                        }
                        dest[j] = '\0';
                    }

                    // Trim trailing whitespace
                    size_t len = strlen(dest);
                    while (len > 0 && (dest[len - 1] == ' ' || dest[len - 1] == '\0')) {
                        dest[--len] = '\0';
                    }

                    if (strlen(dest) > 0) {
                        if (dest == info->title) got_title = true;
                        else if (dest == info->artist) got_artist = true;
                        else if (dest == info->album) got_album = true;
                    }
                }
                free(content);
            }
        }

        pos += 10 + frame_size;
    }

    fclose(f);
    return got_title || got_artist || got_album;
}

/**
 * Read FLAC Vorbis Comments metadata
 * Parses TITLE, ARTIST, ALBUM fields from VORBIS_COMMENT block
 */
static bool read_flac_metadata(const char *path, TrackInfo *info) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    // Check fLaC magic
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "fLaC", 4) != 0) {
        fclose(f);
        return false;
    }

    bool got_title = false, got_artist = false, got_album = false;
    bool last_block = false;

    // Iterate through metadata blocks
    while (!last_block) {
        // Block header (1 byte type + 3 bytes size)
        unsigned char block_header[4];
        if (fread(block_header, 1, 4, f) != 4) break;

        last_block = (block_header[0] & 0x80) != 0;
        int block_type = block_header[0] & 0x7F;
        uint32_t block_size = ((uint32_t)block_header[1] << 16) |
                              ((uint32_t)block_header[2] << 8) |
                              (uint32_t)block_header[3];

        // VORBIS_COMMENT block type = 4
        if (block_type == 4 && block_size > 8) {
            unsigned char *block_data = malloc(block_size);
            if (!block_data) {
                fseek(f, block_size, SEEK_CUR);
                continue;
            }

            if (fread(block_data, 1, block_size, f) != block_size) {
                free(block_data);
                break;
            }

            // Parse Vorbis Comment structure
            uint32_t pos = 0;

            // Vendor string length (little-endian)
            if (pos + 4 > block_size) { free(block_data); continue; }
            uint32_t vendor_len = block_data[pos] |
                                  ((uint32_t)block_data[pos + 1] << 8) |
                                  ((uint32_t)block_data[pos + 2] << 16) |
                                  ((uint32_t)block_data[pos + 3] << 24);
            pos += 4 + vendor_len;

            // Number of comments
            if (pos + 4 > block_size) { free(block_data); continue; }
            uint32_t comment_count = block_data[pos] |
                                     ((uint32_t)block_data[pos + 1] << 8) |
                                     ((uint32_t)block_data[pos + 2] << 16) |
                                     ((uint32_t)block_data[pos + 3] << 24);
            pos += 4;

            // Parse each comment
            for (uint32_t i = 0; i < comment_count && pos + 4 <= block_size; i++) {
                uint32_t comment_len = block_data[pos] |
                                       ((uint32_t)block_data[pos + 1] << 8) |
                                       ((uint32_t)block_data[pos + 2] << 16) |
                                       ((uint32_t)block_data[pos + 3] << 24);
                pos += 4;

                if (pos + comment_len > block_size) break;

                // Comment format: "FIELD=value"
                char *comment = (char *)block_data + pos;

                // Extract field and value
                char *equals = memchr(comment, '=', comment_len);
                if (equals) {
                    size_t field_len = equals - comment;
                    char *value = equals + 1;
                    size_t value_len = comment_len - field_len - 1;

                    // Case-insensitive field comparison
                    char *dest = NULL;
                    size_t dest_size = 0;

                    if (field_len == 5 && strncasecmp(comment, "TITLE", 5) == 0) {
                        dest = info->title;
                        dest_size = sizeof(info->title);
                    } else if (field_len == 6 && strncasecmp(comment, "ARTIST", 6) == 0) {
                        dest = info->artist;
                        dest_size = sizeof(info->artist);
                    } else if (field_len == 5 && strncasecmp(comment, "ALBUM", 5) == 0) {
                        dest = info->album;
                        dest_size = sizeof(info->album);
                    }

                    if (dest && value_len > 0) {
                        size_t copy_len = (value_len < dest_size - 1) ? value_len : dest_size - 1;
                        memcpy(dest, value, copy_len);
                        dest[copy_len] = '\0';

                        if (dest == info->title) got_title = true;
                        else if (dest == info->artist) got_artist = true;
                        else if (dest == info->album) got_album = true;
                    }
                }

                pos += comment_len;
            }

            free(block_data);

            // Found what we need, stop parsing
            if (got_title && got_artist && got_album) break;
        } else {
            // Skip other block types
            fseek(f, block_size, SEEK_CUR);
        }
    }

    fclose(f);
    return got_title || got_artist || got_album;
}

/**
 * Simple ID3v1 tag reader (fallback for old MP3s)
 */
static bool read_id3v1(const char *path, TrackInfo *info) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    if (fseek(f, -128, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    char tag[4];
    if (fread(tag, 1, 3, f) != 3) {
        fclose(f);
        return false;
    }
    tag[3] = '\0';

    if (strcmp(tag, "TAG") != 0) {
        fclose(f);
        return false;
    }

    char title[31], artist[31], album[31];
    if (fread(title, 1, 30, f) != 30 ||
        fread(artist, 1, 30, f) != 30 ||
        fread(album, 1, 30, f) != 30) {
        fclose(f);
        return false;
    }
    title[30] = artist[30] = album[30] = '\0';
    fclose(f);

    // Trim trailing spaces
    for (int i = 29; i >= 0 && title[i] == ' '; i--) title[i] = '\0';
    for (int i = 29; i >= 0 && artist[i] == ' '; i--) artist[i] = '\0';
    for (int i = 29; i >= 0 && album[i] == ' '; i--) album[i] = '\0';

    if (strlen(title) > 0) strncpy(info->title, title, sizeof(info->title) - 1);
    if (strlen(artist) > 0) strncpy(info->artist, artist, sizeof(info->artist) - 1);
    if (strlen(album) > 0) strncpy(info->album, album, sizeof(info->album) - 1);

    return strlen(title) > 0;
}

int audio_init(void) {
    g_music = NULL;
    g_is_paused = false;
    g_start_time = 0;
    g_pause_time = 0;
    g_current_path[0] = '\0';

    memset(&g_track_info, 0, sizeof(g_track_info));
    Mix_VolumeMusic((int)(g_volume * 1.28));

    return 0;
}

void audio_cleanup(void) {
    audio_stop();
}

bool audio_load(const char *path) {
    audio_stop();

    // Store path for potential FLAC seek
    strncpy(g_current_path, path, sizeof(g_current_path) - 1);
    g_current_path[sizeof(g_current_path) - 1] = '\0';

    bool is_flac = is_flac_file(path);

    // For FLAC files, use dr_flac decoder (SDL_mixer on Trimui lacks FLAC support)
    if (is_flac) {
        if (!load_flac_from_position(path, 0)) {
            fprintf(stderr, "[AUDIO] FLAC decode failed: %s\n", path);
            return false;
        }
    } else {
        // For MP3/OGG, use native SDL_mixer
        g_music = Mix_LoadMUS(path);
        if (!g_music) {
            fprintf(stderr, "[AUDIO] Failed to load %s: %s\n", path, Mix_GetError());
            return false;
        }
    }

    // Reset track info
    memset(&g_track_info, 0, sizeof(g_track_info));

    // Priority order for metadata:
    // 1. MusicBrainz cache (from metadata scanner)
    // 2. Embedded tags (ID3v2, Vorbis Comments, ID3v1)
    // 3. Filename extraction

    bool got_metadata = false;

    // Check MusicBrainz cache first
    MetadataResult cached;
    if (metadata_get_cached(path, &cached)) {
        strncpy(g_track_info.title, cached.title, sizeof(g_track_info.title) - 1);
        strncpy(g_track_info.artist, cached.artist, sizeof(g_track_info.artist) - 1);
        strncpy(g_track_info.album, cached.album, sizeof(g_track_info.album) - 1);
        got_metadata = true;
    }

    // If no cache, try embedded tags
    if (!got_metadata) {
        if (is_flac) {
            // FLAC: read Vorbis Comments
            got_metadata = read_flac_metadata(path, &g_track_info);
        } else {
            // MP3/other: try ID3v2 first (modern tags), then ID3v1 (legacy fallback)
            got_metadata = read_id3v2(path, &g_track_info);
            if (!got_metadata) {
                got_metadata = read_id3v1(path, &g_track_info);
            }
        }
    }

    // Final fallback: use filename
    if (!got_metadata) {
        extract_filename_title(path, g_track_info.title, sizeof(g_track_info.title));
        strcpy(g_track_info.artist, "Unknown Artist");
        strcpy(g_track_info.album, "Unknown Album");
    } else {
        // Fill in missing fields even if we got some metadata
        if (strlen(g_track_info.title) == 0) {
            extract_filename_title(path, g_track_info.title, sizeof(g_track_info.title));
        }
        if (strlen(g_track_info.artist) == 0) {
            strcpy(g_track_info.artist, "Unknown Artist");
        }
        if (strlen(g_track_info.album) == 0) {
            strcpy(g_track_info.album, "Unknown Album");
        }
    }

    // Get duration
    g_track_info.duration_sec = 0;

    // For FLAC, we have the duration from decoding
    if (is_flac && g_flac_duration > 0) {
        g_track_info.duration_sec = g_flac_duration;
    }

    // Try SDL_mixer 2.6+ Mix_MusicDuration
#if SDL_MIXER_COMPILEDVERSION >= SDL_VERSIONNUM(2, 6, 0)
    if (g_track_info.duration_sec == 0) {
        double duration = Mix_MusicDuration(g_music);
        if (duration > 0) {
            g_track_info.duration_sec = (int)duration;
        }
    }
#endif

    // Fallback: estimate MP3 duration
    if (g_track_info.duration_sec == 0) {
        const char *ext = strrchr(path, '.');
        if (ext && (strcasecmp(ext, ".mp3") == 0)) {
            int estimated = estimate_mp3_duration(path);
            if (estimated > 0) {
                g_track_info.duration_sec = estimated;
            }
        }
    }

    g_track_info.position_sec = 0;
    g_music_position = 0.0;

    printf("[AUDIO] Loaded: %s - %s (%d sec)\n", g_track_info.artist, g_track_info.title, g_track_info.duration_sec);

    return true;
}

void audio_play(void) {
    if (!g_music) return;

    if (g_is_paused) {
        Mix_ResumeMusic();
        g_start_time += SDL_GetTicks() - g_pause_time;
        g_is_paused = false;
    } else {
        if (Mix_PlayMusic(g_music, 1) < 0) {
            fprintf(stderr, "[AUDIO] Failed to play: %s\n", Mix_GetError());
            return;
        }
        g_start_time = SDL_GetTicks();
        g_music_position = 0.0;
    }
}

void audio_pause(void) {
    if (g_music && Mix_PlayingMusic()) {
        Mix_PauseMusic();
        g_pause_time = SDL_GetTicks();
        g_is_paused = true;
    }
}

void audio_toggle_pause(void) {
    if (g_is_paused) {
        audio_play();
    } else {
        audio_pause();
    }
}

void audio_stop(void) {
    if (g_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(g_music);
        g_music = NULL;
    }

    free_flac_buffer();
    g_current_path[0] = '\0';

    g_is_paused = false;
    g_start_time = 0;
    g_pause_time = 0;
    g_music_position = 0.0;
}

bool audio_is_playing(void) {
    return g_music && Mix_PlayingMusic() && !g_is_paused;
}

bool audio_is_paused(void) {
    return g_music && g_is_paused;
}

void audio_seek(int seconds) {
    if (!g_music) return;

    double new_pos = g_music_position + seconds;
    if (new_pos < 0) new_pos = 0;

    // Clamp to duration
    if (g_track_info.duration_sec > 0 && new_pos >= g_track_info.duration_sec) {
        new_pos = g_track_info.duration_sec - 1;
    }

    // For FLAC (loaded as WAV), we need to reload from the new position
    // because Mix_SetMusicPosition doesn't work with WAV
    if (g_flac_wav_data != NULL && g_current_path[0] != '\0') {
        bool was_playing = audio_is_playing();
        int total_duration = g_flac_duration;  // Save before reload

        // Stop and free current
        Mix_HaltMusic();
        Mix_FreeMusic(g_music);
        g_music = NULL;
        free_flac_buffer();

        // Reload from new position
        if (load_flac_from_position(g_current_path, (int)new_pos)) {
            g_flac_duration = total_duration;  // Restore total duration
            g_track_info.duration_sec = total_duration;

            if (was_playing) {
                Mix_PlayMusic(g_music, 1);
            }

            g_music_position = new_pos;
            g_start_time = SDL_GetTicks() - (Uint32)(new_pos * 1000);
        }
        return;
    }

    // For MP3/OGG, use native seek
    if (Mix_SetMusicPosition(new_pos) == 0) {
        g_music_position = new_pos;
        g_start_time = SDL_GetTicks() - (Uint32)(new_pos * 1000);
    }
}

void audio_set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    g_volume = volume;
    Mix_VolumeMusic((int)(g_volume * 1.28));
}

int audio_get_volume(void) {
    return g_volume;
}

void audio_update(void) {
    if (!g_music || !Mix_PlayingMusic() || g_is_paused) return;

    Uint32 elapsed = SDL_GetTicks() - g_start_time;
    g_music_position = elapsed / 1000.0;
    g_track_info.position_sec = (int)g_music_position;
}

const TrackInfo* audio_get_track_info(void) {
    return &g_track_info;
}

const int16_t* audio_get_pcm_data(size_t *sample_count, int *channels, int *sample_rate) {
    // PCM data no longer exposed (waveform feature removed)
    (void)sample_count;
    (void)channels;
    (void)sample_rate;
    return NULL;
}

bool audio_has_pcm_data(void) {
    return false;
}
