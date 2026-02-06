/**
 * Mono - Minimalist MP3 Player for Trimui Brick
 *
 * A lightweight music player designed for the Trimui Brick handheld
 * running NextUI/MinUI custom firmware.
 *
 * Author: Berckan Guerrero
 * License: MIT
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <stdbool.h>
#include <time.h>

#include "audio.h"
#include "ui.h"
#include "browser.h"
#include "input.h"
#include "menu.h"
#include "state.h"
#include "favorites.h"
#include "screen.h"
#include "sysinfo.h"
#include "positions.h"
#include "cover.h"
#include "filemenu.h"
#include "theme.h"
#include "metadata.h"
#include "youtube.h"
#include "ytsearch.h"
#include "download_queue.h"
#include "equalizer.h"
#include "preload.h"

// Screen dimensions (auto-detected at runtime)
static int g_screen_width = 1280;   // Fallback
static int g_screen_height = 720;   // Fallback

// Application states
typedef enum {
    STATE_HOME,         // Home menu (Resume, Browse, Favorites)
    STATE_RESUME,       // Resume list (tracks with saved positions)
    STATE_FAVORITES,    // Favorites list
    STATE_BROWSER,
    STATE_LOADING,      // Loading a file
    STATE_PLAYING,
    STATE_MENU,
    STATE_HELP_BROWSER,
    STATE_HELP_PLAYER,
    STATE_FILE_MENU,    // File options (delete, rename)
    STATE_CONFIRM,      // Confirmation dialog
    STATE_RENAME,       // Rename text input
    STATE_SCANNING,     // Scanning metadata (MusicBrainz)
    STATE_SCAN_COMPLETE,// Scan finished
    STATE_YOUTUBE_SEARCH,   // YouTube search input
    STATE_YOUTUBE_RESULTS,  // YouTube results list
    STATE_YOUTUBE_DOWNLOAD, // YouTube download progress
    STATE_DOWNLOAD_QUEUE,   // Download queue list view
    STATE_SEEKING,      // Seeking in FLAC file (shows loading)
    STATE_EQUALIZER,    // Equalizer screen (bass/treble horizontal bars)
    STATE_ERROR,        // Error loading file
    STATE_RESUME_PROMPT // Ask user if they want to resume from saved position
} AppState;

// Global state
static bool g_running = true;
static AppState g_state = STATE_HOME;  // Start at home menu
static char g_error_message[256] = {0};  // Last error message
static char g_loading_file[256] = {0};   // File being loaded

// Home menu state
typedef enum { HOME_RESUME, HOME_BROWSE, HOME_FAVORITES, HOME_YOUTUBE, HOME_COUNT } HomeItem;
static int g_home_cursor = HOME_BROWSE;  // Default to Browse

// Resume list state
static int g_resume_cursor = 0;
static int g_resume_scroll = 0;

// Favorites list state
static int g_favorites_cursor = 0;
static int g_favorites_scroll = 0;

// Metadata scanning state
static char g_scan_folder[512] = {0};
static int g_scan_current = 0;
static int g_scan_total = 0;
static int g_scan_found = 0;
static char g_scan_current_file[256] = {0};
static bool g_scan_cancelled = false;

// Currently playing track path (for state persistence)
static char g_current_track_path[512] = {0};

// Seek target for STATE_SEEKING (-1 = none)
static int g_seek_target = -1;

// Pending resume prompt state
static int g_pending_resume_pos = 0;  // Saved position to resume from

// Equalizer band selection (0-4)
static int g_eq_band = 0;

// Joystick for input (raw joystick API - Trimui has incorrect Game Controller mapping)
static SDL_Joystick *g_joystick = NULL;

// Visible rows for resume/favorites lists
#define LIST_VISIBLE_ROWS 8

/**
 * Getters for home/resume/favorites state (used by UI)
 */
int home_get_cursor(void) { return g_home_cursor; }
int resume_get_cursor(void) { return g_resume_cursor; }
int resume_get_scroll(void) { return g_resume_scroll; }
int favorites_get_cursor(void) { return g_favorites_cursor; }
int favorites_get_scroll(void) { return g_favorites_scroll; }
int eq_get_selected_band(void) { return g_eq_band; }

/**
 * Render callback for YouTube download progress
 * Called from within blocking download to update UI
 */
static void download_render_callback(void) {
    ui_render_youtube_download();
    SDL_PumpEvents();  // Allow cancel detection via B button
}

/**
 * Initialize SDL2 and all subsystems
 */
static int init_sdl(void) {
    // Initialize SDL with video, audio, joystick, and events
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // Enable joystick events (needed for both joystick and game controller)
    SDL_JoystickEventState(SDL_ENABLE);

    // Open joystick - use raw joystick API for Trimui Brick
    // Note: SDL Game Controller API has incorrect mapping for "TRIMUI Player1",
    // so we force raw joystick mode which has correct button indices.
    if (SDL_NumJoysticks() > 0) {
        g_joystick = SDL_JoystickOpen(0);
        if (g_joystick) {
            const char *name = SDL_JoystickName(g_joystick);
            printf("Joystick: %s\n", name);
            printf("Axes: %d, Buttons: %d, Hats: %d\n",
                   SDL_JoystickNumAxes(g_joystick),
                   SDL_JoystickNumButtons(g_joystick),
                   SDL_JoystickNumHats(g_joystick));
        }
    } else {
        printf("No joystick found, using keyboard\n");
    }

    // Initialize input system (opens power button device on Linux)
    input_init();

    // Initialize SDL_ttf for font rendering
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return -1;
    }

    // Initialize SDL_mixer for audio playback
    // 44100 Hz (CD quality), signed 16-bit, stereo, 2048 sample buffer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Mix_OpenAudio failed: %s\n", Mix_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    return 0;
}

/**
 * Save current playback position for the current track
 */
static void save_current_position(void) {
    if (g_current_track_path[0]) {
        const TrackInfo *info = audio_get_track_info();
        if (info && info->position_sec > 0) {
            positions_set(g_current_track_path, info->position_sec);
        }
    }
}

/**
 * Load and play a file, restoring saved position if available
 * @param path Path to the audio file
 * @return true if loaded successfully
 */
static bool play_file(const char *path) {
    if (!path) return false;

    // Save position of currently playing track before switching
    save_current_position();

    // Store filename for loading display
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    strncpy(g_loading_file, filename, sizeof(g_loading_file) - 1);

    // Check if we have preloaded data for this track (gapless playback)
    PreloadedTrack *preloaded = preload_consume(path);
    bool loaded = false;

    if (preloaded && preloaded->is_flac && preloaded->wav_data) {
        // Use preloaded FLAC data for instant transition
        loaded = audio_load_preloaded(path, preloaded->wav_data, preloaded->wav_size,
                                       preloaded->duration_sec);
        if (loaded) {
            // wav_data ownership transferred, only free struct
            preloaded->wav_data = NULL;
            printf("[GAPLESS] Used preloaded data for: %s\n", filename);
        }
        preload_free_track(preloaded);
    }

    if (!loaded) {
        // Normal load (no preload or preload failed)
        if (!audio_load(path)) {
            snprintf(g_error_message, sizeof(g_error_message), "Cannot play: %s", filename);
            return false;
        }
    }

    // Update current track path
    strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);

    // Load cover art from track's directory
    const char *dir = browser_get_current_path();
    if (dir) {
        cover_load(dir);
    }

    // Check for saved position - don't auto-seek, let user decide
    int saved_pos = positions_get(path);
    g_pending_resume_pos = saved_pos;  // Store for resume prompt (0 = no saved position)

    // Only start playback if no saved position (otherwise wait for user prompt response)
    if (saved_pos <= 0) {
        audio_play();
    }
    // If saved_pos > 0, playback starts in STATE_RESUME_PROMPT handler

    // Start preloading next track for gapless playback
    const char *next_path = browser_get_next_track_path();
    if (next_path) {
        preload_start(next_path);
    }

    return true;
}

/**
 * Save current application state before exit
 */
static void save_app_state(void) {
    // Save position of current track
    save_current_position();

    AppStateData state_data = {0};

    // Copy current track path
    if (g_current_track_path[0]) {
        strncpy(state_data.last_file, g_current_track_path, sizeof(state_data.last_file) - 1);
    }

    // Copy current folder
    const char *folder = browser_get_current_path();
    if (folder) {
        strncpy(state_data.last_folder, folder, sizeof(state_data.last_folder) - 1);
    }

    // Get playback position
    const TrackInfo *info = audio_get_track_info();
    state_data.last_position = info ? info->position_sec : 0;

    // Get browser cursor
    state_data.last_cursor = browser_get_cursor();

    // Get user preferences
    state_data.volume = audio_get_volume();
    state_data.shuffle = menu_is_shuffle_enabled();
    state_data.repeat = menu_get_repeat_mode();
    state_data.theme = theme_get_current();
    state_data.power_mode = menu_get_power_mode();
    for (int i = 0; i < EQ_BAND_COUNT; i++) {
        state_data.eq_bands[i] = eq_get_band_db(i);
    }

    // Was playing?
    state_data.was_playing = audio_is_playing() || audio_is_paused();

    state_save(&state_data);
}

/**
 * Cleanup all SDL resources
 */
static void cleanup(void) {
    // Save state before cleanup
    save_app_state();

    // Save favorites, positions, and restore screen
    positions_cleanup();
    favorites_cleanup();
    state_cleanup();
    screen_cleanup();
    sysinfo_cleanup();

    metadata_cleanup();
    dlqueue_shutdown();
    youtube_cleanup();
    preload_cleanup();
    eq_cleanup();
    audio_cleanup();
    ui_cleanup();
    browser_cleanup();

    input_cleanup();

    if (g_joystick) {
        SDL_JoystickClose(g_joystick);
    }

    Mix_CloseAudio();
    TTF_Quit();
    SDL_Quit();
}

/**
 * Handle input events based on current state
 */
// Track previous state for returning from help and menu
static AppState g_prev_state = STATE_BROWSER;
static AppState g_menu_return_state = STATE_BROWSER;  // Where to return after menu closes

static void handle_input(AppState *state) {
    SDL_Event event;

    // Poll for hold actions (Y button hold detection)
    InputAction hold_action = input_poll_holds();
    if (hold_action == INPUT_HELP) {
        if (*state == STATE_BROWSER) {
            g_prev_state = STATE_BROWSER;
            *state = STATE_HELP_BROWSER;
        } else if (*state == STATE_PLAYING) {
            g_prev_state = STATE_PLAYING;
            *state = STATE_HELP_PLAYER;
        }
    }

    // Poll for seek (Left/Right D-pad held) - only in player state
    if (*state == STATE_PLAYING && input_is_seeking()) {
        int seek_amount = input_get_seek_amount();
        if (seek_amount != 0) {
            audio_seek(seek_amount);
        }
    }

    // Poll power button (reads from /dev/input/event1 on Linux)
    InputAction power_action = input_poll_power();
    if (power_action == INPUT_SUSPEND) {
        bool was_playing = audio_is_playing();
        if (was_playing) audio_toggle_pause();
        screen_system_suspend();
        input_drain_power();  // Clear accumulated events after wake
        if (was_playing) audio_toggle_pause();
    }

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_running = false;
            return;
        }

        InputAction action = input_handle_event(&event);

        // Global exit handler (Start + B combo)
        if (action == INPUT_EXIT) {
            g_running = false;
            return;
        }

        // Global suspend handler (power button)
        if (action == INPUT_SUSPEND) {
            bool was_playing = audio_is_playing();
            if (was_playing) audio_toggle_pause();
            screen_system_suspend();
            if (was_playing) audio_toggle_pause();
            continue;
        }

        // Help overlay dismissal (X or H to close, like menu)
        if (*state == STATE_HELP_BROWSER || *state == STATE_HELP_PLAYER) {
            // X/H or Back closes help (toggle behavior like menu)
            if (action == INPUT_HELP || action == INPUT_BACK) {
                *state = g_prev_state;
            }
            continue;  // Don't process other actions while in help
        }

        // Error state dismissal (any button press)
        if (*state == STATE_ERROR) {
            if (action != INPUT_NONE) {
                *state = STATE_BROWSER;
                g_error_message[0] = '\0';
            }
            continue;
        }

        // Loading state - no input handling, just wait
        if (*state == STATE_LOADING) {
            continue;
        }

        // Confirm dialog handling (separate state like help overlay)
        if (*state == STATE_CONFIRM) {
            if (action == INPUT_SELECT) {
                FileMenuResult result = filemenu_confirm_delete(true);
                if (result == FILEMENU_RESULT_DELETED) {
                    browser_rescan_preserve_cursor();
                }
                *state = STATE_BROWSER;
            } else if (action == INPUT_BACK) {
                filemenu_confirm_delete(false);
                *state = STATE_FILE_MENU;  // Back to file menu
            }
            continue;  // Don't process other actions in confirm
        }

        // Resume prompt handling (ask user before seeking to saved position)
        if (*state == STATE_RESUME_PROMPT) {
            if (action == INPUT_SELECT) {
                // A = Resume from saved position
                // Use STATE_SEEKING to do the seek after a frame (timing issues on Trimui)
                audio_play();
                g_seek_target = g_pending_resume_pos;
                g_pending_resume_pos = 0;
                *state = STATE_SEEKING;  // Will show "Seeking..." and do seek next frame
            } else if (action == INPUT_BACK) {
                // B = Start from beginning
                audio_play();
                g_pending_resume_pos = 0;
                *state = STATE_PLAYING;
            }
            continue;
        }

        // Home menu handling
        if (*state == STATE_HOME) {
            switch (action) {
                case INPUT_UP:
                    g_home_cursor = (g_home_cursor + HOME_COUNT - 1) % HOME_COUNT;
                    break;
                case INPUT_DOWN:
                    g_home_cursor = (g_home_cursor + 1) % HOME_COUNT;
                    break;
                case INPUT_SELECT:
                    switch (g_home_cursor) {
                        case HOME_RESUME:
                            if (positions_get_count() > 0) {
                                g_resume_cursor = 0;
                                g_resume_scroll = 0;
                                *state = STATE_RESUME;
                            }
                            break;
                        case HOME_BROWSE:
                            *state = STATE_BROWSER;
                            break;
                        case HOME_FAVORITES:
                            if (favorites_get_count() > 0) {
                                g_favorites_cursor = 0;
                                g_favorites_scroll = 0;
                                *state = STATE_FAVORITES;
                            }
                            break;
                        case HOME_YOUTUBE:
                            if (youtube_is_available()) {
                                ytsearch_init();
                                ytsearch_set_render_callback(download_render_callback);
                                *state = STATE_YOUTUBE_SEARCH;
                            }
                            break;
                    }
                    break;
                case INPUT_MENU:
                    g_menu_return_state = STATE_HOME;
                    menu_open(MENU_MODE_BROWSER);
                    *state = STATE_MENU;
                    break;
                default:
                    break;
            }
            continue;
        }

        // Resume list handling
        if (*state == STATE_RESUME) {
            int count = positions_get_count();
            switch (action) {
                case INPUT_UP:
                    if (g_resume_cursor > 0) {
                        g_resume_cursor--;
                        if (g_resume_cursor < g_resume_scroll) {
                            g_resume_scroll = g_resume_cursor;
                        }
                    }
                    break;
                case INPUT_DOWN:
                    if (g_resume_cursor < count - 1) {
                        g_resume_cursor++;
                        if (g_resume_cursor >= g_resume_scroll + LIST_VISIBLE_ROWS) {
                            g_resume_scroll = g_resume_cursor - LIST_VISIBLE_ROWS + 1;
                        }
                    }
                    break;
                case INPUT_SELECT: {
                    // Play selected track from saved position
                    // Disable favorites playback mode - we're now playing from resume
                    favorites_set_playback_mode(false, 0);
                    char path[512];
                    int pos = positions_get_entry(g_resume_cursor, path, sizeof(path));
                    if (pos >= 0 && path[0]) {
                        // Navigate browser to file's directory
                        char dir[512];
                        strncpy(dir, path, sizeof(dir) - 1);
                        char *last_slash = strrchr(dir, '/');
                        if (last_slash) {
                            *last_slash = '\0';
                            browser_navigate_to(dir);
                            // Find and select the file
                            const char *filename = last_slash + 1;
                            int browser_count = browser_get_count();
                            for (int i = 0; i < browser_count; i++) {
                                const FileEntry *entry = browser_get_entry(i);
                                if (entry && strcmp(entry->name, filename) == 0) {
                                    browser_set_cursor(i);
                                    break;
                                }
                            }
                        }
                        if (play_file(path)) {
                            *state = (g_pending_resume_pos > 0) ? STATE_RESUME_PROMPT : STATE_PLAYING;
                        }
                    }
                    break;
                }
                case INPUT_FAVORITE:
                    // Y = remove from resume list
                    if (count > 0) {
                        char path[512];
                        if (positions_get_entry(g_resume_cursor, path, sizeof(path)) >= 0) {
                            positions_clear(path);
                            // Adjust cursor if needed
                            int new_count = positions_get_count();
                            if (g_resume_cursor >= new_count && new_count > 0) {
                                g_resume_cursor = new_count - 1;
                            }
                            if (new_count == 0) {
                                *state = STATE_HOME;
                            }
                        }
                    }
                    break;
                case INPUT_BACK:
                    *state = STATE_HOME;
                    break;
                default:
                    break;
            }
            continue;
        }

        // Favorites list handling
        if (*state == STATE_FAVORITES) {
            int count = favorites_get_count();
            switch (action) {
                case INPUT_UP:
                    if (g_favorites_cursor > 0) {
                        g_favorites_cursor--;
                        if (g_favorites_cursor < g_favorites_scroll) {
                            g_favorites_scroll = g_favorites_cursor;
                        }
                    }
                    break;
                case INPUT_DOWN:
                    if (g_favorites_cursor < count - 1) {
                        g_favorites_cursor++;
                        if (g_favorites_cursor >= g_favorites_scroll + LIST_VISIBLE_ROWS) {
                            g_favorites_scroll = g_favorites_cursor - LIST_VISIBLE_ROWS + 1;
                        }
                    }
                    break;
                case INPUT_SELECT: {
                    // Play selected favorite
                    const char *path = favorites_get_path(g_favorites_cursor);
                    if (path) {
                        // Enable favorites playback mode
                        favorites_set_playback_mode(true, g_favorites_cursor);

                        // Still navigate browser for display purposes
                        char dir[512];
                        strncpy(dir, path, sizeof(dir) - 1);
                        char *last_slash = strrchr(dir, '/');
                        if (last_slash) {
                            *last_slash = '\0';
                            browser_navigate_to(dir);
                            const char *filename = last_slash + 1;
                            int browser_count = browser_get_count();
                            for (int i = 0; i < browser_count; i++) {
                                const FileEntry *entry = browser_get_entry(i);
                                if (entry && strcmp(entry->name, filename) == 0) {
                                    browser_set_cursor(i);
                                    break;
                                }
                            }
                        }
                        if (play_file(path)) {
                            *state = (g_pending_resume_pos > 0) ? STATE_RESUME_PROMPT : STATE_PLAYING;
                        }
                    }
                    break;
                }
                case INPUT_FAVORITE:
                    // Y = remove from favorites
                    if (count > 0) {
                        const char *path = favorites_get_path(g_favorites_cursor);
                        if (path) {
                            favorites_remove(path);
                            // Adjust cursor if needed
                            int new_count = favorites_get_count();
                            if (g_favorites_cursor >= new_count && new_count > 0) {
                                g_favorites_cursor = new_count - 1;
                            }
                            if (new_count == 0) {
                                *state = STATE_HOME;
                            }
                        }
                    }
                    break;
                case INPUT_BACK:
                    *state = STATE_HOME;
                    break;
                default:
                    break;
            }
            continue;
        }

        // File menu handling
        if (*state == STATE_FILE_MENU) {
            switch (action) {
                case INPUT_UP:
                    filemenu_move_cursor(-1);
                    break;
                case INPUT_DOWN:
                    filemenu_move_cursor(1);
                    break;
                case INPUT_SELECT:
                    if (filemenu_select()) {
                        int option = filemenu_get_actual_option();
                        if (option == FILEMENU_RENAME) {
                            filemenu_rename_init();
                            *state = STATE_RENAME;
                        } else if (option == FILEMENU_SCAN_METADATA) {
                            // Initialize metadata scan
                            strncpy(g_scan_folder, filemenu_get_path(), sizeof(g_scan_folder) - 1);
                            g_scan_current = 0;
                            g_scan_found = 0;
                            g_scan_cancelled = false;
                            g_scan_current_file[0] = '\0';

                            // Count audio files in folder
                            g_scan_total = 0;
                            DIR *dir = opendir(g_scan_folder);
                            if (dir) {
                                struct dirent *entry;
                                while ((entry = readdir(dir)) != NULL) {
                                    if (entry->d_name[0] == '.') continue;
                                    const char *ext = strrchr(entry->d_name, '.');
                                    if (ext && (strcasecmp(ext, ".mp3") == 0 ||
                                                strcasecmp(ext, ".flac") == 0 ||
                                                strcasecmp(ext, ".ogg") == 0 ||
                                                strcasecmp(ext, ".wav") == 0)) {
                                        g_scan_total++;
                                    }
                                }
                                closedir(dir);
                            }

                            *state = STATE_SCANNING;
                        } else if (option == FILEMENU_CANCEL) {
                            *state = STATE_BROWSER;
                        }
                    } else {
                        // Delete selected - transition to confirm state (like help overlay)
                        *state = STATE_CONFIRM;
                    }
                    break;
                case INPUT_BACK:
                    *state = STATE_BROWSER;
                    break;
                default:
                    break;
            }
            continue;
        }

        switch (*state) {
            case STATE_BROWSER:
                switch (action) {
                    case INPUT_UP:
                        browser_move_cursor(-1);
                        break;
                    case INPUT_DOWN:
                        browser_move_cursor(1);
                        break;
                    case INPUT_SELECT:
                        if (browser_select_current()) {
                            // File selected, start playing (with position restore)
                            // Disable favorites playback mode - we're now playing from browser
                            favorites_set_playback_mode(false, 0);
                            const char *path = browser_get_selected_path();
                            if (play_file(path)) {
                                *state = (g_pending_resume_pos > 0) ? STATE_RESUME_PROMPT : STATE_PLAYING;
                            } else {
                                // Show error state (will auto-return to browser)
                                *state = STATE_ERROR;
                            }
                        }
                        break;
                    case INPUT_BACK:
                        // Navigate up, or return to HOME if at root
                        if (!browser_go_up()) {
                            *state = STATE_HOME;
                        }
                        break;
                    case INPUT_FAVORITE: {
                        // Toggle favorite for selected file
                        const char *path = browser_get_selected_path();
                        const FileEntry *entry = browser_get_entry(browser_get_cursor());
                        if (path && entry && entry->type == ENTRY_FILE) {
                            bool is_now_fav = favorites_toggle(path);
                            printf("[MAIN] %s %s favorites\n", path,
                                   is_now_fav ? "added to" : "removed from");
                        }
                        break;
                    }
                    case INPUT_VOL_UP:
                        audio_set_volume(audio_get_volume() + 5);
                        break;
                    case INPUT_VOL_DOWN:
                        audio_set_volume(audio_get_volume() - 5);
                        break;
                    case INPUT_HELP:
                        g_prev_state = STATE_BROWSER;
                        *state = STATE_HELP_BROWSER;
                        break;
                    case INPUT_SHUFFLE: {
                        // Open file menu for selected item (not parent "..")
                        const FileEntry *entry = browser_get_entry(browser_get_cursor());
                        if (entry && entry->type != ENTRY_PARENT) {
                            filemenu_init(entry->full_path, entry->type == ENTRY_DIRECTORY);
                            *state = STATE_FILE_MENU;
                        }
                        break;
                    }
                    case INPUT_MENU:
                        // Open options menu from browser
                        g_menu_return_state = STATE_BROWSER;
                        menu_open(MENU_MODE_BROWSER);
                        *state = STATE_MENU;
                        break;
                    default:
                        break;
                }
                break;

            case STATE_PLAYING:
                // Restore screen brightness on any input (if dimmed)
                // Exception: INPUT_SHUFFLE (SELECT button) toggles dim
                if (action != INPUT_NONE && action != INPUT_SHUFFLE && screen_is_dimmed()) {
                    screen_restore();
                }

                switch (action) {
                    case INPUT_SELECT:
                        audio_toggle_pause();
                        break;
                    case INPUT_SHUFFLE:
                        // In player mode, SELECT = screen dim toggle
                        screen_toggle_dim();
                        break;
                    case INPUT_BACK:
                        // Save position before going back to browser
                        save_current_position();
                        audio_stop();
                        ui_player_reset_scroll();
                        g_current_track_path[0] = '\0';  // Clear current track
                        *state = STATE_BROWSER;
                        break;
                    // INPUT_LEFT/INPUT_RIGHT: handled by hold-to-seek with acceleration
                    case INPUT_UP:
                        audio_set_volume(audio_get_volume() + 5);
                        sysinfo_refresh_volume();
                        break;
                    case INPUT_DOWN:
                        audio_set_volume(audio_get_volume() - 5);
                        sysinfo_refresh_volume();
                        break;
                    case INPUT_PREV:
                        // Play previous track (with position save/restore)
                        if (favorites_is_playback_mode()) {
                            favorites_advance_playback(-1);
                            const char *path = favorites_get_current_playback_path();
                            if (path) {
                                g_favorites_cursor = favorites_get_playback_index();
                                play_file(path);
                            }
                        } else if (browser_move_cursor(-1)) {
                            const char *path = browser_get_selected_path();
                            play_file(path);
                        }
                        break;
                    case INPUT_NEXT:
                        // Play next track (with position save/restore)
                        if (favorites_is_playback_mode()) {
                            favorites_advance_playback(1);
                            const char *path = favorites_get_current_playback_path();
                            if (path) {
                                g_favorites_cursor = favorites_get_playback_index();
                                play_file(path);
                            }
                        } else if (browser_move_cursor(1)) {
                            const char *path = browser_get_selected_path();
                            play_file(path);
                        }
                        break;
                    case INPUT_FAVORITE:
                        // Toggle favorite for current track
                        if (g_current_track_path[0]) {
                            bool is_now_fav = favorites_toggle(g_current_track_path);
                            printf("[MAIN] %s %s favorites\n", g_current_track_path,
                                   is_now_fav ? "added to" : "removed from");
                        }
                        break;
                    case INPUT_MENU:
                        g_menu_return_state = STATE_PLAYING;
                        menu_open(MENU_MODE_PLAYER);
                        *state = STATE_MENU;
                        break;
                    case INPUT_VOL_UP:
                        audio_set_volume(audio_get_volume() + 5);
                        break;
                    case INPUT_VOL_DOWN:
                        audio_set_volume(audio_get_volume() - 5);
                        break;
                    case INPUT_HELP:
                        g_prev_state = STATE_PLAYING;
                        *state = STATE_HELP_PLAYER;
                        break;
                    case INPUT_SEEK_START:
                        // L2 - jump to beginning of track
                        if (audio_is_flac()) {
                            // FLAC: use STATE_SEEKING for clean loading display
                            g_seek_target = 0;
                            *state = STATE_SEEKING;
                        } else {
                            audio_seek_absolute(0);
                        }
                        break;
                    case INPUT_SEEK_END: {
                        // R2 - jump near end of track (5 seconds before end)
                        const TrackInfo *info = audio_get_track_info();
                        if (info && info->duration_sec > 5) {
                            if (audio_is_flac()) {
                                // FLAC: use STATE_SEEKING for clean loading display
                                g_seek_target = info->duration_sec - 5;
                                *state = STATE_SEEKING;
                            } else {
                                audio_seek_absolute(info->duration_sec - 5);
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
                break;

            case STATE_MENU:
                switch (action) {
                    case INPUT_UP:
                        menu_move_cursor(-1);
                        break;
                    case INPUT_DOWN:
                        menu_move_cursor(1);
                        break;
                    case INPUT_SELECT: {
                        MenuResult result = menu_select();
                        if (result == MENU_RESULT_EQUALIZER) {
                            g_eq_band = 0;  // Start on Bass
                            *state = STATE_EQUALIZER;
                        } else if (result == MENU_RESULT_CLOSE) {
                            *state = g_menu_return_state;
                        }
                        // MENU_RESULT_NONE: stay in menu
                        break;
                    }
                    case INPUT_BACK:
                        // Return to previous state (browser or player)
                        *state = g_menu_return_state;
                        break;
                    case INPUT_VOL_UP:
                        audio_set_volume(audio_get_volume() + 5);
                        break;
                    case INPUT_VOL_DOWN:
                        audio_set_volume(audio_get_volume() - 5);
                        break;
                    default:
                        break;
                }
                break;

            case STATE_EQUALIZER:
                switch (action) {
                    case INPUT_LEFT:
                        // Select previous band
                        if (g_eq_band > 0) g_eq_band--;
                        break;
                    case INPUT_RIGHT:
                        // Select next band
                        if (g_eq_band < EQ_BAND_COUNT - 1) g_eq_band++;
                        break;
                    case INPUT_UP:
                        // Increase selected band level
                        eq_adjust_band(g_eq_band, 1);
                        state_notify_settings_changed();
                        break;
                    case INPUT_DOWN:
                        // Decrease selected band level
                        eq_adjust_band(g_eq_band, -1);
                        state_notify_settings_changed();
                        break;
                    case INPUT_SELECT:
                        // A = Reset to flat
                        eq_reset();
                        state_notify_settings_changed();
                        break;
                    case INPUT_BACK:
                        // B = Back to menu
                        *state = STATE_MENU;
                        break;
                    default:
                        break;
                }
                break;

            case STATE_HELP_BROWSER:
            case STATE_HELP_PLAYER:
                // Handled above with continue statement
                break;

            case STATE_FILE_MENU:
                // Handled above with continue statement (early-exit pattern)
                break;

            case STATE_RENAME:
                switch (action) {
                    case INPUT_UP:
                        filemenu_rename_move_kbd(0, -1);  // Grid: up
                        break;
                    case INPUT_DOWN:
                        filemenu_rename_move_kbd(0, 1);   // Grid: down
                        break;
                    case INPUT_LEFT:
                        filemenu_rename_move_kbd(-1, 0);  // Grid: left
                        break;
                    case INPUT_RIGHT:
                        filemenu_rename_move_kbd(1, 0);   // Grid: right
                        break;
                    case INPUT_SELECT:
                        filemenu_rename_insert();
                        break;
                    case INPUT_FAVORITE:
                    case INPUT_BACK:
                        // Y or B = delete character (backspace)
                        filemenu_rename_delete();
                        break;
                    case INPUT_MENU: {
                        // Start = confirm rename
                        FileMenuResult result = filemenu_rename_confirm();
                        if (result == FILEMENU_RESULT_RENAMED) {
                            browser_navigate_to(browser_get_current_path());
                        }
                        *state = STATE_BROWSER;
                        break;
                    }
                    case INPUT_SHUFFLE:
                        // Select = cancel
                        *state = STATE_BROWSER;
                        break;
                    default:
                        break;
                }
                break;

            case STATE_CONFIRM:
                // Handled earlier with dedicated if-block
                break;

            case STATE_RESUME_PROMPT:
                // Handled earlier with dedicated if-block
                break;

            case STATE_SCANNING:
                // B to cancel scanning
                if (action == INPUT_BACK) {
                    g_scan_cancelled = true;
                    *state = STATE_SCAN_COMPLETE;
                }
                break;

            case STATE_SCAN_COMPLETE:
                // Any button returns to browser
                if (action == INPUT_SELECT || action == INPUT_BACK) {
                    *state = STATE_BROWSER;
                }
                break;

            case STATE_YOUTUBE_SEARCH:
                switch (action) {
                    case INPUT_UP:
                        ytsearch_move_kbd(0, -1);   // Grid: up
                        break;
                    case INPUT_DOWN:
                        ytsearch_move_kbd(0, 1);    // Grid: down
                        break;
                    case INPUT_LEFT:
                        ytsearch_move_kbd(-1, 0);   // Grid: left
                        break;
                    case INPUT_RIGHT:
                        ytsearch_move_kbd(1, 0);    // Grid: right
                        break;
                    case INPUT_SELECT:
                        ytsearch_insert();
                        break;
                    case INPUT_FAVORITE:
                    case INPUT_BACK:
                        // Y or B = delete character (backspace)
                        ytsearch_delete();
                        break;
                    case INPUT_MENU:
                        // Start = execute search
                        if (ytsearch_execute_search()) {
                            // State will change to SEARCHING in update()
                        }
                        break;
                    case INPUT_SHUFFLE:
                        // Select = cancel
                        *state = STATE_BROWSER;
                        break;
                    default:
                        break;
                }
                break;

            case STATE_YOUTUBE_RESULTS:
                switch (action) {
                    case INPUT_UP:
                        ytsearch_move_results_cursor(-1);
                        break;
                    case INPUT_DOWN:
                        ytsearch_move_results_cursor(1);
                        break;
                    case INPUT_SELECT: {
                        // Add selected result to download queue (background download)
                        const YouTubeResult *result = ytsearch_get_result(ytsearch_get_results_cursor());
                        if (result) {
                            if (dlqueue_add(result->id, result->title, result->channel)) {
                                printf("[MAIN] Added to queue: %s\n", result->title);
                                // Show toast notification
                                char toast[128];
                                int pending = dlqueue_pending_count();
                                snprintf(toast, sizeof(toast), "Added to queue (%d pending)", pending);
                                ui_show_toast(toast);
                            } else {
                                // Already in queue or queue full
                                if (dlqueue_is_queued(result->id)) {
                                    ui_show_toast("Already in queue");
                                } else {
                                    ui_show_toast("Queue full (max 20)");
                                }
                            }
                        }
                        // Stay in results - user can add more
                        break;
                    }
                    case INPUT_HELP:
                        // X button - open download queue view
                        dlqueue_view_init();
                        *state = STATE_DOWNLOAD_QUEUE;
                        break;
                    case INPUT_BACK:
                        // Go back to search input
                        ytsearch_set_state(YTSEARCH_INPUT);
                        *state = STATE_YOUTUBE_SEARCH;
                        break;
                    default:
                        break;
                }
                break;

            case STATE_DOWNLOAD_QUEUE:
                switch (action) {
                    case INPUT_UP:
                        dlqueue_view_move_cursor(-1);
                        break;
                    case INPUT_DOWN:
                        dlqueue_view_move_cursor(1);
                        break;
                    case INPUT_SELECT: {
                        // Play completed item
                        if (dlqueue_view_action_select()) {
                            const char *filepath = dlqueue_view_get_selected_path();
                            if (filepath) {
                                // Navigate browser to downloaded file's directory and play
                                char dir[512];
                                strncpy(dir, filepath, sizeof(dir) - 1);
                                char *last_slash = strrchr(dir, '/');
                                if (last_slash) {
                                    *last_slash = '\0';
                                    browser_navigate_to(dir);
                                    // Find and select the file
                                    const char *filename = last_slash + 1;
                                    int count = browser_get_count();
                                    for (int i = 0; i < count; i++) {
                                        const FileEntry *entry = browser_get_entry(i);
                                        if (entry && strcmp(entry->name, filename) == 0) {
                                            browser_set_cursor(i);
                                            break;
                                        }
                                    }
                                }
                                if (play_file(filepath)) {
                                    *state = (g_pending_resume_pos > 0) ? STATE_RESUME_PROMPT : STATE_PLAYING;
                                }
                            }
                        }
                        break;
                    }
                    case INPUT_HELP:
                        // X button - cancel selected (pending only)
                        if (dlqueue_view_action_cancel()) {
                            ui_show_toast("Download cancelled");
                        }
                        break;
                    case INPUT_FAVORITE:
                        // Y button - clear completed/failed
                        dlqueue_clear_completed();
                        break;
                    case INPUT_BACK:
                        *state = STATE_YOUTUBE_RESULTS;
                        break;
                    default:
                        break;
                }
                break;

            case STATE_YOUTUBE_DOWNLOAD:
                // Legacy - no longer used (downloads are background now)
                *state = STATE_YOUTUBE_RESULTS;
                break;

            case STATE_LOADING:
            case STATE_ERROR:
                // Handled above with continue statement
                break;

            case STATE_HOME:
            case STATE_RESUME:
            case STATE_FAVORITES:
                // Handled above with continue statement
                break;

            case STATE_SEEKING:
                // No input during seek - wait for update() to complete
                break;
        }
    }
}

/**
 * Update game state
 */
static Uint32 g_last_position_save = 0;
static int g_last_saved_position = -1;  // Track last saved value to avoid redundant writes
#define POSITION_SAVE_INTERVAL_MS 15000  // Save position every 15 seconds (was 10)

static void update(AppState *state) {
    // Check power switch (GPIO 243) - poll every 200ms
    static Uint32 last_switch_check = 0;
    Uint32 now = SDL_GetTicks();
    if (now - last_switch_check > 200) {
        last_switch_check = now;
        bool switch_on = screen_switch_is_on();
        if (switch_on && !screen_is_off()) {
            screen_off();
        } else if (!switch_on && screen_is_off()) {
            screen_on();
        }
    }

    // Check sleep timer
    if (*state == STATE_PLAYING || *state == STATE_MENU || *state == STATE_EQUALIZER) {
        if (menu_update_sleep_timer()) {
            // Sleep timer expired - save position and return to browser
            save_current_position();
            audio_stop();
            *state = STATE_BROWSER;
            return;
        }
    }

    // Periodically save position while playing (only if position changed significantly)
    if (*state == STATE_PLAYING && audio_is_playing()) {
        Uint32 now = SDL_GetTicks();
        if (now - g_last_position_save >= POSITION_SAVE_INTERVAL_MS) {
            const TrackInfo *info = audio_get_track_info();
            // Only save if position changed by at least 5 seconds
            if (info && (g_last_saved_position < 0 ||
                         abs(info->position_sec - g_last_saved_position) >= 5)) {
                save_current_position();
                g_last_saved_position = info->position_sec;
            }
            g_last_position_save = now;
        }
    }

    // Check if current track finished (not just paused)
    if (*state == STATE_PLAYING && !audio_is_playing() && !audio_is_paused()) {
        // Track finished completely - clear its saved position
        if (g_current_track_path[0]) {
            positions_clear(g_current_track_path);
        }

        RepeatMode repeat = menu_get_repeat_mode();

        if (repeat == REPEAT_ONE) {
            // Repeat current track from beginning
            const char *path = favorites_is_playback_mode()
                ? favorites_get_current_playback_path()
                : browser_get_selected_path();
            if (path && audio_load(path)) {
                strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                audio_play();
            }
        } else if (favorites_is_playback_mode()) {
            // Favorites playback mode - advance through favorites
            int fav_count = favorites_get_count();
            if (fav_count > 0) {
                const char *path = NULL;
                if (menu_is_shuffle_enabled()) {
                    // Shuffle among favorites
                    int random_idx = rand() % fav_count;
                    favorites_set_playback_index(random_idx);
                    path = favorites_get_current_playback_path();
                } else {
                    // Sequential through favorites
                    int new_idx = favorites_advance_playback(1);
                    if (new_idx == 0 && repeat != REPEAT_ALL) {
                        // Wrapped around and repeat is off - stop
                        g_current_track_path[0] = '\0';
                        *state = STATE_BROWSER;
                        favorites_set_playback_mode(false, 0);
                    } else {
                        path = favorites_get_current_playback_path();
                    }
                }
                if (path && audio_load(path)) {
                    strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                    // Update favorites cursor to match playback position
                    g_favorites_cursor = favorites_get_playback_index();
                    audio_play();
                }
            } else {
                // No favorites, disable playback mode
                favorites_set_playback_mode(false, 0);
                g_current_track_path[0] = '\0';
                *state = STATE_BROWSER;
            }
        } else if (menu_is_shuffle_enabled()) {
            // Shuffle: play random track from browser
            int count = browser_get_count();
            if (count > 0) {
                int random_offset = rand() % count;
                browser_move_cursor(random_offset - browser_get_cursor());
                const char *path = browser_get_selected_path();
                if (path && audio_load(path)) {
                    strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                    audio_play();
                }
            }
        } else {
            // Normal: advance to next track in browser (with gapless support)
            if (browser_move_cursor(1)) {
                const char *path = browser_get_selected_path();
                if (path) {
                    // Try gapless transition first
                    bool loaded = false;
                    PreloadedTrack *preloaded = preload_consume(path);
                    if (preloaded && preloaded->is_flac && preloaded->wav_data) {
                        loaded = audio_load_preloaded(path, preloaded->wav_data,
                                                       preloaded->wav_size, preloaded->duration_sec);
                        if (loaded) {
                            preloaded->wav_data = NULL;  // Ownership transferred
                            printf("[GAPLESS] Seamless transition to: %s\n", path);
                        }
                        preload_free_track(preloaded);
                    }
                    if (!loaded) {
                        loaded = audio_load(path);
                    }
                    if (loaded) {
                        strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                        audio_play();
                        // Preload next track
                        const char *next = browser_get_next_track_path();
                        if (next) preload_start(next);
                    }
                }
            } else if (repeat == REPEAT_ALL) {
                // At end of list, go back to start
                browser_move_cursor(-browser_get_cursor());
                const char *path = browser_get_selected_path();
                if (path && audio_load(path)) {
                    strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                    audio_play();
                    // Preload next track
                    const char *next = browser_get_next_track_path();
                    if (next) preload_start(next);
                }
            } else {
                // No more tracks, return to browser
                g_current_track_path[0] = '\0';
                *state = STATE_BROWSER;
            }
        }
    }

    // Handle metadata scanning (incremental - one file per frame)
    if (*state == STATE_SCANNING) {
        // Get next file to scan
        DIR *dir = opendir(g_scan_folder);
        if (dir) {
            struct dirent *entry;
            int count = 0;
            bool found_file = false;

            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;

                // Check if audio file
                const char *ext = strrchr(entry->d_name, '.');
                if (!ext) continue;
                if (strcasecmp(ext, ".mp3") != 0 && strcasecmp(ext, ".flac") != 0 &&
                    strcasecmp(ext, ".ogg") != 0 && strcasecmp(ext, ".wav") != 0) continue;

                count++;
                if (count <= g_scan_current) continue;  // Skip already processed

                // Found next file to process
                found_file = true;
                g_scan_current = count;
                strncpy(g_scan_current_file, entry->d_name, sizeof(g_scan_current_file) - 1);

                // Build full path
                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s/%s", g_scan_folder, entry->d_name);

                // Lookup metadata
                MetadataResult result;
                if (metadata_lookup(filepath, &result)) {
                    g_scan_found++;
                }
                break;
            }
            closedir(dir);

            // Check if scan complete
            if (!found_file || g_scan_current >= g_scan_total) {
                *state = STATE_SCAN_COMPLETE;
            }
        } else {
            *state = STATE_SCAN_COMPLETE;
        }
    }

    // Handle YouTube search (blocking operation with loading indicator)
    if (*state == STATE_YOUTUBE_SEARCH && ytsearch_get_state() == YTSEARCH_SEARCHING) {
        // Render "Searching..." screen BEFORE blocking search call
        // This ensures user sees the loading indicator
        ui_render_youtube_search();

        if (ytsearch_update_search()) {
            // Search complete
            if (ytsearch_get_state() == YTSEARCH_RESULTS) {
                *state = STATE_YOUTUBE_RESULTS;
            }
            // On error, stays in YOUTUBE_SEARCH (error shown)
        }
    }

    // Check for completed background downloads
    if (dlqueue_has_new_completions()) {
        // A download finished in the background
        // Just log it - user stays in current state
        const char *completed = dlqueue_get_last_completed();
        if (completed) {
            printf("[MAIN] Background download complete: %s\n", completed);
        }
    }

    // Handle seeking (state-based to allow audio to settle before seek)
    // Uses a frame counter to ensure audio has started before seeking
    static int seek_delay_frames = 0;
    if (*state == STATE_SEEKING && g_seek_target >= 0) {
        seek_delay_frames++;
        if (seek_delay_frames >= 3) {  // Wait 3 frames (~100ms) for audio to start
            audio_seek_absolute(g_seek_target);
            g_seek_target = -1;
            seek_delay_frames = 0;
            *state = STATE_PLAYING;
        }
    } else {
        seek_delay_frames = 0;
    }

    // Update audio position (for progress bar)
    audio_update();
}

/**
 * Render current state
 */
static void render(AppState *state) {
    switch (*state) {
        case STATE_HOME:
            ui_render_home();
            break;
        case STATE_RESUME:
            ui_render_resume();
            break;
        case STATE_FAVORITES:
            ui_render_favorites();
            break;
        case STATE_BROWSER:
            ui_render_browser();
            break;
        case STATE_LOADING:
            ui_render_loading(g_loading_file);
            break;
        case STATE_PLAYING:
            ui_render_player();
            break;
        case STATE_MENU:
            ui_render_menu();
            break;
        case STATE_EQUALIZER:
            ui_render_equalizer();
            break;
        case STATE_HELP_BROWSER:
            ui_render_help_browser();
            break;
        case STATE_HELP_PLAYER:
            ui_render_help_player();
            break;
        case STATE_FILE_MENU:
            ui_render_file_menu();
            break;
        case STATE_CONFIRM:
            ui_render_confirm_delete();
            break;
        case STATE_RESUME_PROMPT:
            ui_render_resume_prompt(g_pending_resume_pos);
            break;
        case STATE_RENAME:
            ui_render_rename();
            break;
        case STATE_SCANNING:
            ui_render_scanning(g_scan_current, g_scan_total, g_scan_current_file, g_scan_found);
            break;
        case STATE_SCAN_COMPLETE:
            ui_render_scan_complete(g_scan_found, g_scan_total);
            break;
        case STATE_YOUTUBE_SEARCH:
            ui_render_youtube_search();
            break;
        case STATE_YOUTUBE_RESULTS:
            ui_render_youtube_results();
            break;
        case STATE_YOUTUBE_DOWNLOAD:
            ui_render_youtube_download();
            break;
        case STATE_DOWNLOAD_QUEUE:
            ui_render_download_queue();
            break;
        case STATE_SEEKING:
            ui_render_loading("Seeking...");
            break;
        case STATE_ERROR:
            ui_render_error(g_error_message);
            break;
    }
}

/**
 * Main entry point
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Mono - Starting...\n");

    // Initialize SDL
    if (init_sdl() < 0) {
        return 1;
    }

    // Initialize UI
#ifdef __APPLE__
    // macOS: use fixed Trimui Brick resolution for windowed preview
    g_screen_width = 1280;
    g_screen_height = 720;
    printf("[MAIN] macOS preview mode: %dx%d\n", g_screen_width, g_screen_height);
#else
    // Embedded device: auto-detect screen dimensions
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
        g_screen_width = mode.w;
        g_screen_height = mode.h;
        printf("[MAIN] Detected display: %dx%d\n", g_screen_width, g_screen_height);
    } else {
        printf("[MAIN] Using fallback display: %dx%d\n", g_screen_width, g_screen_height);
    }
#endif

    if (ui_init(g_screen_width, g_screen_height) < 0) {
        fprintf(stderr, "UI initialization failed\n");
        cleanup();
        return 1;
    }

    // Initialize audio engine
    if (audio_init() < 0) {
        fprintf(stderr, "Audio initialization failed\n");
        cleanup();
        return 1;
    }

    // Initialize equalizer (after Mix_OpenAudio)
    eq_init();

    // Initialize preloader for gapless playback
    preload_init();

    // Initialize metadata cache (MusicBrainz lookups)
    metadata_init();

    // Initialize YouTube integration
    youtube_init();

    // Initialize background download queue
    dlqueue_init();

    // Initialize file browser
    // Default music path - can be overridden via command line
    const char *music_path = "/mnt/SDCARD/Music";
    if (argc > 1) {
        music_path = argv[1];
    }

    if (browser_init(music_path) < 0) {
        fprintf(stderr, "Browser initialization failed\n");
        cleanup();
        return 1;
    }

    // Initialize menu system
    menu_init();

    // Initialize theme system
    theme_init();

    // Initialize state persistence
    if (state_init() < 0) {
        fprintf(stderr, "State initialization failed (non-fatal)\n");
    }
    // Register callback to save state when settings change (power mode, etc.)
    state_set_settings_callback(save_app_state);

    // Initialize favorites
    if (favorites_init() < 0) {
        fprintf(stderr, "Favorites initialization failed (non-fatal)\n");
    }

    // Initialize position tracking
    if (positions_init() < 0) {
        fprintf(stderr, "Positions initialization failed (non-fatal)\n");
    }

    // Warm SD card cache for files with saved positions (prevents slow first-seek)
    // Reads entire files to cache them for fast Mix_SetMusicPosition seeking
    {
        int pos_count = positions_get_count();
        int files_to_cache = (pos_count < 10) ? pos_count : 10;
        if (files_to_cache > 0) {
            printf("[MAIN] Warming SD cache for %d files...\n", files_to_cache);
            char path[512];
            unsigned char cache_buf[32768];  // 32KB read buffer
            for (int i = 0; i < files_to_cache; i++) {
                // Show loading progress
                char loading_msg[64];
                snprintf(loading_msg, sizeof(loading_msg), "Loading cache %d/%d...", i + 1, files_to_cache);
                ui_render_loading(loading_msg);

                if (positions_get_entry(i, path, sizeof(path)) >= 0) {
                    FILE *f = fopen(path, "rb");
                    if (f) {
                        // Read entire file to warm cache
                        while (fread(cache_buf, 1, sizeof(cache_buf), f) == sizeof(cache_buf)) {
                            // Just reading to warm cache
                        }
                        fclose(f);
                    }
                }
            }
            printf("[MAIN] SD cache warmed\n");
        }
    }

    // Initialize screen brightness control
    if (screen_init() < 0) {
        fprintf(stderr, "Screen control initialization failed (non-fatal)\n");
    }

    // Initialize system info (battery, volume)
    if (sysinfo_init() < 0) {
        fprintf(stderr, "System info initialization failed (non-fatal)\n");
    }

    // Try to restore previous state
    AppStateData saved_state;
    if (state_load(&saved_state)) {
        // Restore user preferences
        audio_set_volume(saved_state.volume);
        menu_set_shuffle(saved_state.shuffle);
        menu_set_repeat(saved_state.repeat);
        theme_set(saved_state.theme);
        menu_set_power_mode(saved_state.power_mode);
        for (int i = 0; i < EQ_BAND_COUNT; i++) {
            eq_set_band_db(i, saved_state.eq_bands[i]);
        }

        // Try to resume playback if there was an active track
        if (saved_state.has_resume_data && saved_state.last_file[0]) {
            // Navigate to the folder containing the last track
            if (saved_state.last_folder[0]) {
                browser_navigate_to(saved_state.last_folder);
            }

            // Find and select the track
            int count = browser_get_count();
            for (int i = 0; i < count; i++) {
                const FileEntry *entry = browser_get_entry(i);
                if (entry && strcmp(entry->full_path, saved_state.last_file) == 0) {
                    browser_set_cursor(i);
                    break;
                }
            }

            // Auto-resume if was playing
            if (saved_state.was_playing) {
                const char *path = browser_get_selected_path();
                if (play_file(path)) {
                    g_state = STATE_PLAYING;
                }
            }
        }
    }

    // Seed random number generator for shuffle
    srand((unsigned int)time(NULL));

    printf("Mono - Initialized successfully\n");

    // Adaptive frame rate for energy efficiency:
    // Frame rates depend on power mode setting:
    // - Battery:     20fps active, 10fps dimmed
    // - Balanced:    30fps active, 10fps dimmed (default)
    // - Performance: 60fps active, 20fps dimmed
    Uint32 frame_start;
    AppState prev_state = g_state;

    while (g_running) {
        frame_start = SDL_GetTicks();

        // Determine target frame time based on power mode and activity
        Uint32 target_frame_ms;
        PowerMode power = menu_get_power_mode();

        if (screen_is_dimmed()) {
            // Screen dimmed - minimal updates regardless of mode
            target_frame_ms = (power == POWER_MODE_PERFORMANCE) ? 50 : 100;  // 20fps or 10fps
        } else if (g_state == STATE_PLAYING && audio_is_playing()) {
            // Active playback - smooth progress bar & animation
            switch (power) {
                case POWER_MODE_BATTERY:     target_frame_ms = 50;  break;  // 20fps
                case POWER_MODE_BALANCED:    target_frame_ms = 33;  break;  // 30fps
                case POWER_MODE_PERFORMANCE: target_frame_ms = 16;  break;  // 60fps
                default:                     target_frame_ms = 33;  break;
            }
        } else if (g_state == STATE_PLAYING && audio_is_paused()) {
            // Paused - less urgent
            switch (power) {
                case POWER_MODE_BATTERY:     target_frame_ms = 100; break;  // 10fps
                case POWER_MODE_BALANCED:    target_frame_ms = 50;  break;  // 20fps
                case POWER_MODE_PERFORMANCE: target_frame_ms = 33;  break;  // 30fps
                default:                     target_frame_ms = 50;  break;
            }
        } else {
            // Browser/menu - balanced responsiveness
            switch (power) {
                case POWER_MODE_BATTERY:     target_frame_ms = 50;  break;  // 20fps
                case POWER_MODE_BALANCED:    target_frame_ms = 33;  break;  // 30fps
                case POWER_MODE_PERFORMANCE: target_frame_ms = 16;  break;  // 60fps
                default:                     target_frame_ms = 33;  break;
            }
        }

        handle_input(&g_state);
        update(&g_state);
        render(&g_state);

        // Track state changes for debugging
        if (g_state != prev_state) {
            prev_state = g_state;
        }

        // Energy-efficient sleep - use longer delay when less activity needed
        Uint32 frame_duration = SDL_GetTicks() - frame_start;
        if (frame_duration < target_frame_ms) {
            SDL_Delay(target_frame_ms - frame_duration);
        }
    }

    printf("Mono - Shutting down...\n");
    cleanup();

    return 0;
}
