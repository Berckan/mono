/**
 * Input Handler Implementation
 *
 * Maps keyboard/gamepad inputs to abstract actions.
 * Supports both desktop testing (keyboard) and Trimui Brick hardware.
 *
 * Trimui Brick Button Mapping (from NextUI platform.h):
 * - JOY_A = 1 (south button - confirm/select)
 * - JOY_B = 0 (east button - captured by system for launcher)
 * - JOY_X = 3 (west button - back, since B is captured)
 * - JOY_Y = 2 (north button)
 * - JOY_L1 = 4, JOY_R1 = 5 (shoulder buttons)
 * - JOY_SELECT = 6, JOY_START = 7
 * - JOY_MENU = 8
 */

#include "input.h"
#include <stdio.h>
#include <stdbool.h>

// Keyboard mappings for desktop testing
// These mirror the Trimui Brick controls
#define KEY_UP      SDLK_UP
#define KEY_DOWN    SDLK_DOWN
#define KEY_LEFT    SDLK_LEFT
#define KEY_RIGHT   SDLK_RIGHT
#define KEY_A       SDLK_z       // A button (confirm)
#define KEY_B       SDLK_x       // B button (back)
#define KEY_Y       SDLK_f       // Y button (favorite) - F key on keyboard
#define KEY_L       SDLK_a       // L shoulder
#define KEY_R       SDLK_s       // R shoulder
#define KEY_START   SDLK_RETURN  // Start
#define KEY_SELECT  SDLK_RSHIFT  // Select

// Trimui Brick joystick button indices (from NextUI platform.h)
// These match the official NextUI mapping for tg5040 platform
#define JOY_A       1   // South button (confirm)
#define JOY_B       0   // East button (captured by system)
#define JOY_X       3   // West button (back)
#define JOY_Y       2   // North button
#define JOY_L1      4   // Left shoulder
#define JOY_R1      5   // Right shoulder
#define JOY_SELECT  6
#define JOY_START   7
#define JOY_MENU    8

// Button state tracking for debouncing (gamepad doesn't filter repeats like keyboard)
static Uint8 g_button_state[16] = {0};

// Time-based cooldown for buttons that need it (prevents physical bounce)
static Uint32 g_button_cooldown[16] = {0};
#define BUTTON_COOLDOWN_MS 250  // Minimum ms between button presses

// Y button hold tracking for help overlay
static Uint32 g_y_press_time = 0;
static bool g_y_held = false;
#define Y_HOLD_THRESHOLD_MS 300  // Hold Y for 300ms to show help

// Hat (D-Pad) state tracking - only trigger on state CHANGE
static Uint8 g_hat_state = SDL_HAT_CENTERED;

InputAction input_handle_event(const SDL_Event *event) {
    switch (event->type) {
        case SDL_KEYDOWN:
            // Ignore key repeats
            if (event->key.repeat) return INPUT_NONE;

            printf("[KEY] sym=%d\n", event->key.keysym.sym);

            switch (event->key.keysym.sym) {
                case KEY_UP:     return INPUT_UP;
                case KEY_DOWN:   return INPUT_DOWN;
                case KEY_LEFT:   return INPUT_LEFT;
                case KEY_RIGHT:  return INPUT_RIGHT;
                case KEY_A:      return INPUT_SELECT;
                case KEY_B:      return INPUT_BACK;
                case KEY_Y:      return INPUT_FAVORITE;
                case KEY_L:      return INPUT_PREV;
                case KEY_R:      return INPUT_NEXT;
                case KEY_START:  return INPUT_MENU;
                case KEY_SELECT: return INPUT_SHUFFLE;
                case SDLK_ESCAPE:return INPUT_BACK;
                // Hardware volume keys (Trimui Brick)
                case SDLK_VOLUMEUP:   return INPUT_VOL_UP;
                case SDLK_VOLUMEDOWN: return INPUT_VOL_DOWN;
                default: break;
            }
            break;

        // Raw Joystick API - used for Trimui Brick (Game Controller mapping is incorrect)
        case SDL_JOYBUTTONDOWN: {
            int btn = event->jbutton.button;
            Uint32 now = SDL_GetTicks();

            // Debouncing: ignore if button already pressed
            if (btn < 16 && g_button_state[btn]) {
                return INPUT_NONE;
            }

            // Time-based cooldown for Start/Menu buttons (physical bounce prevention)
            if (btn < 16 && (btn == JOY_START || btn == JOY_MENU)) {
                if (now - g_button_cooldown[btn] < BUTTON_COOLDOWN_MS) {
                    return INPUT_NONE;  // Too soon, ignore
                }
                g_button_cooldown[btn] = now;
            }

            if (btn < 16) g_button_state[btn] = 1;

            printf("[BTN] button=%d\n", btn);
            fflush(stdout);

            // TRIMUI Brick button mapping (from NextUI platform.h)
            // Note: JOY_B (index 0) is captured by system for "back to launcher"
            switch (btn) {
                case JOY_A:      return INPUT_SELECT;   // A - confirm/play-pause
                case JOY_X:      return INPUT_BACK;     // X - back (B captured by system)
                case JOY_Y:
                    // Track Y press time for hold detection
                    g_y_press_time = now;
                    g_y_held = false;
                    return INPUT_NONE;  // Don't trigger on press, wait for hold check or release
                case JOY_L1:     return INPUT_PREV;     // L1 - previous track
                case JOY_R1:     return INPUT_NEXT;     // R1 - next track
                case JOY_SELECT: return INPUT_SHUFFLE;  // Select - shuffle/dim toggle
                case JOY_START:  return INPUT_MENU;     // Start - menu
                case JOY_MENU:   return INPUT_MENU;     // Menu button
                default: break;
            }
            break;
        }

        case SDL_JOYBUTTONUP: {
            int btn = event->jbutton.button;
            if (btn < 16) g_button_state[btn] = 0;

            // Y button release: if not held long enough, it's a tap (favorite)
            if (btn == JOY_Y) {
                if (!g_y_held && g_y_press_time > 0) {
                    Uint32 now = SDL_GetTicks();
                    if (now - g_y_press_time < Y_HOLD_THRESHOLD_MS) {
                        g_y_press_time = 0;
                        return INPUT_FAVORITE;  // Quick tap = toggle favorite
                    }
                }
                g_y_press_time = 0;
                g_y_held = false;
            }
            return INPUT_NONE;
        }

        case SDL_JOYHATMOTION: {
            Uint8 new_state = event->jhat.value;

            // Only trigger on state CHANGE (debouncing)
            if (new_state == g_hat_state) {
                return INPUT_NONE;
            }

            Uint8 old_state = g_hat_state;
            g_hat_state = new_state;

            // Only trigger when a direction is NEWLY pressed
            // (not when released or when already held)
            if (new_state == SDL_HAT_CENTERED) {
                return INPUT_NONE;  // Released, no action
            }

            printf("[HAT] value=%d (was %d)\n", new_state, old_state);
            fflush(stdout);

            switch (new_state) {
                case SDL_HAT_UP:    return INPUT_UP;
                case SDL_HAT_DOWN:  return INPUT_DOWN;
                case SDL_HAT_LEFT:  return INPUT_LEFT;
                case SDL_HAT_RIGHT: return INPUT_RIGHT;
                default: break;
            }
            break;
        }

        case SDL_JOYAXISMOTION:
            // Only log significant axis movements
            if (abs(event->jaxis.value) > 16000) {
                printf("[AXIS] axis=%d value=%d\n", event->jaxis.axis, event->jaxis.value);
                if (event->jaxis.axis == 0) {  // X axis
                    return (event->jaxis.value < 0) ? INPUT_LEFT : INPUT_RIGHT;
                } else if (event->jaxis.axis == 1) {  // Y axis
                    return (event->jaxis.value < 0) ? INPUT_UP : INPUT_DOWN;
                }
            }
            break;

        default:
            break;
    }

    return INPUT_NONE;
}

InputAction input_poll_holds(void) {
    // Check if Y is being held past threshold
    if (g_y_press_time > 0 && !g_y_held && g_button_state[JOY_Y]) {
        Uint32 now = SDL_GetTicks();
        if (now - g_y_press_time >= Y_HOLD_THRESHOLD_MS) {
            g_y_held = true;
            return INPUT_HELP;
        }
    }
    return INPUT_NONE;
}
