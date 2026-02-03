/**
 * Audio Engine Implementation
 *
 * Uses SDL_mixer for audio playback with support for MP3, FLAC, and OGG.
 * Provides basic ID3 tag extraction for metadata display.
 */

#include "audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Current track
static Mix_Music *g_music = NULL;
static TrackInfo g_track_info;
static bool g_is_paused = false;
static int g_volume = 80;  // Default 80%

// Track start time for position calculation
static Uint32 g_start_time = 0;
static Uint32 g_pause_time = 0;
static double g_music_position = 0.0;

/**
 * Extract filename without extension as fallback title
 */
static void extract_filename_title(const char *path, char *title, size_t size) {
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++;  // Skip the '/'
    } else {
        filename = path;
    }

    // Copy filename
    strncpy(title, filename, size - 1);
    title[size - 1] = '\0';

    // Remove extension
    char *dot = strrchr(title, '.');
    if (dot) {
        *dot = '\0';
    }
}

/**
 * Simple ID3v1 tag reader (128 bytes at end of file)
 * ID3v1 format: TAG + Title(30) + Artist(30) + Album(30) + Year(4) + Comment(30) + Genre(1)
 */
static bool read_id3v1(const char *path, TrackInfo *info) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    // Seek to ID3v1 tag position (128 bytes from end)
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

    // Check for "TAG" marker
    if (strcmp(tag, "TAG") != 0) {
        fclose(f);
        return false;
    }

    // Read title (30 bytes)
    char title[31];
    if (fread(title, 1, 30, f) != 30) {
        fclose(f);
        return false;
    }
    title[30] = '\0';

    // Read artist (30 bytes)
    char artist[31];
    if (fread(artist, 1, 30, f) != 30) {
        fclose(f);
        return false;
    }
    artist[30] = '\0';

    // Read album (30 bytes)
    char album[31];
    if (fread(album, 1, 30, f) != 30) {
        fclose(f);
        return false;
    }
    album[30] = '\0';

    fclose(f);

    // Trim trailing spaces
    for (int i = 29; i >= 0 && title[i] == ' '; i--) title[i] = '\0';
    for (int i = 29; i >= 0 && artist[i] == ' '; i--) artist[i] = '\0';
    for (int i = 29; i >= 0 && album[i] == ' '; i--) album[i] = '\0';

    // Only use if we got actual data
    if (strlen(title) > 0) strncpy(info->title, title, sizeof(info->title) - 1);
    if (strlen(artist) > 0) strncpy(info->artist, artist, sizeof(info->artist) - 1);
    if (strlen(album) > 0) strncpy(info->album, album, sizeof(info->album) - 1);

    return strlen(title) > 0;
}

int audio_init(void) {
    // SDL_mixer should already be initialized in main
    // Just reset our state
    g_music = NULL;
    g_is_paused = false;
    g_start_time = 0;
    g_pause_time = 0;

    memset(&g_track_info, 0, sizeof(g_track_info));

    // Set initial volume
    Mix_VolumeMusic((int)(g_volume * 1.28));  // Scale to 0-128

    return 0;
}

void audio_cleanup(void) {
    audio_stop();
}

bool audio_load(const char *path) {
    // Stop current playback
    audio_stop();

    // Load the new track
    g_music = Mix_LoadMUS(path);
    if (!g_music) {
        fprintf(stderr, "Failed to load %s: %s\n", path, Mix_GetError());
        return false;
    }

    // Reset track info
    memset(&g_track_info, 0, sizeof(g_track_info));

    // Extract metadata
    // First try ID3 tags
    if (!read_id3v1(path, &g_track_info)) {
        // Fallback to filename
        extract_filename_title(path, g_track_info.title, sizeof(g_track_info.title));
        strcpy(g_track_info.artist, "Unknown Artist");
        strcpy(g_track_info.album, "Unknown Album");
    }

    // Duration unknown without SDL_mixer 2.6+ (Mix_MusicDuration)
    // We'll estimate based on elapsed time during playback
    g_track_info.duration_sec = 0;

    g_track_info.position_sec = 0;
    g_music_position = 0.0;

    printf("Loaded: %s - %s\n", g_track_info.artist, g_track_info.title);

    return true;
}

void audio_play(void) {
    if (!g_music) return;

    if (g_is_paused) {
        // Resume from pause
        Mix_ResumeMusic();
        g_start_time += SDL_GetTicks() - g_pause_time;
        g_is_paused = false;
    } else {
        // Start fresh
        if (Mix_PlayMusic(g_music, 1) < 0) {
            fprintf(stderr, "Failed to play music: %s\n", Mix_GetError());
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

    // Mix_SetMusicPosition works for MP3/OGG in older SDL_mixer
    if (Mix_SetMusicPosition(new_pos) == 0) {
        g_music_position = new_pos;
        g_start_time = SDL_GetTicks() - (Uint32)(new_pos * 1000);
    }
}

void audio_set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    g_volume = volume;

    // SDL_mixer volume is 0-128
    Mix_VolumeMusic((int)(g_volume * 1.28));
}

int audio_get_volume(void) {
    return g_volume;
}

void audio_update(void) {
    if (!g_music || !Mix_PlayingMusic() || g_is_paused) return;

    // Calculate current position
    Uint32 elapsed = SDL_GetTicks() - g_start_time;
    g_music_position = elapsed / 1000.0;
    g_track_info.position_sec = (int)g_music_position;
}

const TrackInfo* audio_get_track_info(void) {
    return &g_track_info;
}
