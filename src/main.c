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

// Screen dimensions (auto-detected at runtime)
static int g_screen_width = 1280;   // Fallback
static int g_screen_height = 720;   // Fallback

// Application states
typedef enum {
    STATE_BROWSER,
    STATE_PLAYING,
    STATE_MENU,
    STATE_HELP_BROWSER,
    STATE_HELP_PLAYER
} AppState;

// Global state
static bool g_running = true;
static AppState g_state = STATE_BROWSER;

// Currently playing track path (for state persistence)
static char g_current_track_path[512] = {0};

// Joystick for input (raw joystick API - Trimui has incorrect Game Controller mapping)
static SDL_Joystick *g_joystick = NULL;

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
 * Save current application state before exit
 */
static void save_app_state(void) {
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

    // Save favorites and restore screen
    favorites_cleanup();
    state_cleanup();
    screen_cleanup();

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
// Track previous state for returning from help
static AppState g_prev_state = STATE_BROWSER;

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

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_running = false;
            return;
        }

        InputAction action = input_handle_event(&event);

        // Help overlay dismissal (Y released)
        if (*state == STATE_HELP_BROWSER || *state == STATE_HELP_PLAYER) {
            // Any button release returns to previous state
            if (event.type == SDL_JOYBUTTONUP || event.type == SDL_KEYUP) {
                *state = g_prev_state;
            }
            continue;  // Don't process other actions while in help
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
                            // File selected, start playing
                            const char *path = browser_get_selected_path();
                            if (path && audio_load(path)) {
                                // Store current track path for state persistence
                                strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                                audio_play();
                                *state = STATE_PLAYING;
                            }
                        }
                        break;
                    case INPUT_BACK:
                        if (!browser_go_up()) {
                            // At root, exit application
                            g_running = false;
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
                        audio_stop();
                        g_current_track_path[0] = '\0';  // Clear current track
                        *state = STATE_BROWSER;
                        break;
                    case INPUT_LEFT:
                        audio_seek(-10);  // Seek back 10 seconds
                        break;
                    case INPUT_RIGHT:
                        audio_seek(10);   // Seek forward 10 seconds
                        break;
                    case INPUT_UP:
                        // D-Pad UP = volume up (hardware vol keys captured by system)
                        audio_set_volume(audio_get_volume() + 5);
                        break;
                    case INPUT_DOWN:
                        // D-Pad DOWN = volume down
                        audio_set_volume(audio_get_volume() - 5);
                        break;
                    case INPUT_PREV:
                        // Play previous track
                        if (browser_move_cursor(-1)) {
                            const char *path = browser_get_selected_path();
                            if (path && audio_load(path)) {
                                strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                                audio_play();
                            }
                        }
                        break;
                    case INPUT_NEXT:
                        // Play next track
                        if (browser_move_cursor(1)) {
                            const char *path = browser_get_selected_path();
                            if (path && audio_load(path)) {
                                strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                                audio_play();
                            }
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
                            // Exit selected - stop playback and return to browser
                            audio_stop();
                            *state = STATE_BROWSER;
                        }
                        break;
                    case INPUT_BACK:
                        // Only X closes menu (Start was causing rapid open/close)
                        *state = STATE_PLAYING;
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
        }
    }
}

/**
 * Update game state
 */
static void update(AppState *state) {
    // Check sleep timer
    if (*state == STATE_PLAYING || *state == STATE_MENU) {
        if (menu_update_sleep_timer()) {
            // Sleep timer expired - stop playback and return to browser
            audio_stop();
            *state = STATE_BROWSER;
            return;
        }
    }

    // Check if current track finished (not just paused)
    if (*state == STATE_PLAYING && !audio_is_playing() && !audio_is_paused()) {
        RepeatMode repeat = menu_get_repeat_mode();

        if (repeat == REPEAT_ONE) {
            // Repeat current track
            const char *path = browser_get_selected_path();
            if (path && audio_load(path)) {
                strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);
                audio_play();
            }
        } else if (menu_is_shuffle_enabled()) {
            // Shuffle: play random track
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
            // Normal: advance to next track
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
    // Auto-detect screen dimensions
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
        g_screen_width = mode.w;
        g_screen_height = mode.h;
        printf("[MAIN] Detected display: %dx%d\n", g_screen_width, g_screen_height);
    } else {
        printf("[MAIN] Using fallback display: %dx%d\n", g_screen_width, g_screen_height);
    }

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

    // Initialize state persistence
    if (state_init() < 0) {
        fprintf(stderr, "State initialization failed (non-fatal)\n");
    }

    // Initialize favorites
    if (favorites_init() < 0) {
        fprintf(stderr, "Favorites initialization failed (non-fatal)\n");
    }

    // Initialize screen brightness control
    if (screen_init() < 0) {
        fprintf(stderr, "Screen control initialization failed (non-fatal)\n");
    }

    // Try to restore previous state
    AppStateData saved_state;
    if (state_load(&saved_state)) {
        // Restore user preferences
        audio_set_volume(saved_state.volume);
        menu_set_shuffle(saved_state.shuffle);
        menu_set_repeat(saved_state.repeat);

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
                if (path && audio_load(path)) {
                    strncpy(g_current_track_path, path, sizeof(g_current_track_path) - 1);

                    // Seek to saved position
                    if (saved_state.last_position > 0) {
                        audio_play();
                        audio_seek(saved_state.last_position);
                    } else {
                        audio_play();
                    }
                    g_state = STATE_PLAYING;
                    printf("[MAIN] Resumed playback: %s @ %ds\n",
                           path, saved_state.last_position);
                }
            }
        }
    }

    // Seed random number generator for shuffle
    srand((unsigned int)time(NULL));

    printf("Mono - Initialized successfully\n");

    // Main loop
    while (g_running) {
        handle_input(&g_state);
        update(&g_state);
        render(&g_state);

        // Cap at ~60fps
        SDL_Delay(16);
    }

    printf("Mono - Shutting down...\n");
    cleanup();

    return 0;
}
