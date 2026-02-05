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

// Screen dimensions (auto-detected at runtime)
static int g_screen_width = 1280;   // Fallback
static int g_screen_height = 720;   // Fallback

// Application states
typedef enum {
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
    STATE_ERROR         // Error loading file
} AppState;

// Global state
static bool g_running = true;
static AppState g_state = STATE_BROWSER;
static char g_error_message[256] = {0};  // Last error message
static char g_loading_file[256] = {0};   // File being loaded

// Metadata scanning state
static char g_scan_folder[512] = {0};
static int g_scan_current = 0;
static int g_scan_total = 0;
static int g_scan_found = 0;
static char g_scan_current_file[256] = {0};
static bool g_scan_cancelled = false;

// Currently playing track path (for state persistence)
static char g_current_track_path[512] = {0};

// Joystick for input (raw joystick API - Trimui has incorrect Game Controller mapping)
static SDL_Joystick *g_joystick = NULL;

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

    // Load new file
    if (!audio_load(path)) {
        // Store error message
        snprintf(g_error_message, sizeof(g_error_message), "Cannot play: %s", filename);
        return false;
    }

    // Update current track path
    strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);

    // Load cover art from track's directory
    const char *dir = browser_get_current_path();
    if (dir) {
        cover_load(dir);
    }

    // Check for saved position
    int saved_pos = positions_get(path);
    if (saved_pos > 0) {
        audio_play();
        audio_seek(saved_pos);
        printf("[MAIN] Resumed %s at %d seconds\n", path, saved_pos);
    } else {
        audio_play();
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
    audio_cleanup();
    ui_cleanup();
    browser_cleanup();

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
                    browser_navigate_to(browser_get_current_path());
                }
                *state = STATE_BROWSER;
            } else if (action == INPUT_BACK) {
                filemenu_confirm_delete(false);
                *state = STATE_FILE_MENU;  // Back to file menu
            }
            continue;  // Don't process other actions in confirm
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
                            const char *path = browser_get_selected_path();
                            if (play_file(path)) {
                                *state = STATE_PLAYING;
                            } else {
                                // Show error state (will auto-return to browser)
                                *state = STATE_ERROR;
                            }
                        }
                        break;
                    case INPUT_BACK:
                        // Just navigate up, don't exit (use Start+B to exit)
                        browser_go_up();
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
                        if (browser_move_cursor(-1)) {
                            const char *path = browser_get_selected_path();
                            play_file(path);
                        }
                        break;
                    case INPUT_NEXT:
                        // Play next track (with position save/restore)
                        if (browser_move_cursor(1)) {
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
                        audio_seek_absolute(0);
                        break;
                    case INPUT_SEEK_END: {
                        // R2 - jump near end of track (5 seconds before end)
                        const TrackInfo *info = audio_get_track_info();
                        if (info && info->duration_sec > 5) {
                            audio_seek_absolute(info->duration_sec - 5);
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
                    case INPUT_SELECT:
                        if (menu_select()) {
                            // Check if YouTube was selected
                            if (menu_youtube_selected()) {
                                menu_reset_youtube();
                                // Initialize YouTube search and switch state
                                ytsearch_init();
                                ytsearch_set_render_callback(download_render_callback);
                                *state = STATE_YOUTUBE_SEARCH;
                            } else {
                                // Exit selected - stop playback (if playing) and return to browser
                                if (g_menu_return_state == STATE_PLAYING) {
                                    audio_stop();
                                }
                                *state = STATE_BROWSER;
                            }
                        }
                        break;
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
                    case INPUT_BACK:
                        *state = STATE_BROWSER;
                        break;
                    default:
                        break;
                }
                break;

            case STATE_CONFIRM:
                // Handled within STATE_FILE_MENU
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
                        ytsearch_delete();
                        break;
                    case INPUT_MENU:
                        // Start = execute search
                        if (ytsearch_execute_search()) {
                            // State will change to SEARCHING in update()
                        }
                        break;
                    case INPUT_BACK:
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
                                    *state = STATE_PLAYING;
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
    // Check sleep timer
    if (*state == STATE_PLAYING || *state == STATE_MENU) {
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
            const char *path = browser_get_selected_path();
            if (path && audio_load(path)) {
                strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                audio_play();
            }
        } else if (menu_is_shuffle_enabled()) {
            // Shuffle: play random track from beginning
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
            // Normal: advance to next track from beginning
            if (browser_move_cursor(1)) {
                const char *path = browser_get_selected_path();
                if (path && audio_load(path)) {
                    strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                    audio_play();
                }
            } else if (repeat == REPEAT_ALL) {
                // At end of list, go back to start
                browser_move_cursor(-browser_get_cursor());
                const char *path = browser_get_selected_path();
                if (path && audio_load(path)) {
                    strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                    audio_play();
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

    // Update audio position (for progress bar)
    audio_update();
}

/**
 * Render current state
 */
static void render(AppState *state) {
    switch (*state) {
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

    // Initialize favorites
    if (favorites_init() < 0) {
        fprintf(stderr, "Favorites initialization failed (non-fatal)\n");
    }

    // Initialize position tracking
    if (positions_init() < 0) {
        fprintf(stderr, "Positions initialization failed (non-fatal)\n");
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
