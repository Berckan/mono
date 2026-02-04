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

// Screen dimensions for Trimui Brick
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Application states
typedef enum {
    STATE_BROWSER,
    STATE_PLAYING,
    STATE_MENU
} AppState;

// Global state
static bool g_running = true;
static AppState g_state = STATE_BROWSER;

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
    // 44100 Hz, signed 16-bit, stereo, 2048 sample buffer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Mix_OpenAudio failed: %s\n", Mix_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    return 0;
}

/**
 * Cleanup all SDL resources
 */
static void cleanup(void) {
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
static void handle_input(AppState *state) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_running = false;
            return;
        }

        InputAction action = input_handle_event(&event);

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

            case STATE_PLAYING:
                switch (action) {
                    case INPUT_SELECT:
                        audio_toggle_pause();
                        break;
                    case INPUT_BACK:
                        audio_stop();
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
                                audio_play();
                            }
                        }
                        break;
                    case INPUT_NEXT:
                        // Play next track
                        if (browser_move_cursor(1)) {
                            const char *path = browser_get_selected_path();
                            if (path && audio_load(path)) {
                                audio_play();
                            }
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
                    case INPUT_MENU:
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
                    audio_play();
                }
            }
        } else {
            // Normal: advance to next track
            if (browser_move_cursor(1)) {
                const char *path = browser_get_selected_path();
                if (path && audio_load(path)) {
                    audio_play();
                }
            } else if (repeat == REPEAT_ALL) {
                // At end of list, go back to start
                browser_move_cursor(-browser_get_cursor());
                const char *path = browser_get_selected_path();
                if (path && audio_load(path)) {
                    audio_play();
                }
            } else {
                // No more tracks, return to browser
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
    if (ui_init(SCREEN_WIDTH, SCREEN_HEIGHT) < 0) {
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
