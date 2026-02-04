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

#include "audio.h"
#include "ui.h"
#include "browser.h"
#include "input.h"

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

// Controller/Joystick for input
// Prefer Game Controller API (standardized button mapping)
// Fall back to raw joystick if no mapping available
static SDL_GameController *g_controller = NULL;
static SDL_Joystick *g_joystick = NULL;

/**
 * Initialize SDL2 and all subsystems
 */
static int init_sdl(void) {
    // Initialize SDL with video, audio, joystick, game controller, and events
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // Enable joystick events (needed for both joystick and game controller)
    SDL_JoystickEventState(SDL_ENABLE);

    // Try to open as Game Controller first (standardized button mapping)
    // Falls back to raw joystick if no mapping available in gamecontrollerdb.txt
    if (SDL_NumJoysticks() > 0) {
        if (SDL_IsGameController(0)) {
            g_controller = SDL_GameControllerOpen(0);
            if (g_controller) {
                printf("Controller: %s\n", SDL_GameControllerName(g_controller));
                printf("Mapping: %s\n", SDL_GameControllerMapping(g_controller));
            }
        } else {
            // Fallback to raw joystick (device-specific button indices)
            g_joystick = SDL_JoystickOpen(0);
            if (g_joystick) {
                printf("Joystick (no mapping): %s\n", SDL_JoystickName(g_joystick));
                printf("Axes: %d, Buttons: %d, Hats: %d\n",
                       SDL_JoystickNumAxes(g_joystick),
                       SDL_JoystickNumButtons(g_joystick),
                       SDL_JoystickNumHats(g_joystick));
            }
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

    if (g_controller) {
        SDL_GameControllerClose(g_controller);
    }
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
    // Check if current track finished
    if (*state == STATE_PLAYING && !audio_is_playing()) {
        // Auto-advance to next track
        if (browser_move_cursor(1)) {
            const char *path = browser_get_selected_path();
            if (path && audio_load(path)) {
                audio_play();
            }
        } else {
            // No more tracks, return to browser
            *state = STATE_BROWSER;
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
