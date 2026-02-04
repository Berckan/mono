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

// Keyboard mappings for desktop testing
// These mirror the Trimui Brick controls
#define KEY_UP      SDLK_UP
#define KEY_DOWN    SDLK_DOWN
#define KEY_LEFT    SDLK_LEFT
#define KEY_RIGHT   SDLK_RIGHT
#define KEY_A       SDLK_z       // A button (confirm)
#define KEY_B       SDLK_x       // B button (back)
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
            printf("[BTN] button=%d\n", btn);
            fflush(stdout);

            // Debouncing: ignore if button already pressed
            if (btn < 16 && g_button_state[btn]) {
                return INPUT_NONE;
            }
            if (btn < 16) g_button_state[btn] = 1;

            // TRIMUI Brick button mapping (from NextUI platform.h)
            // Note: JOY_B (index 0) is captured by system for "back to launcher"
            switch (btn) {
                case JOY_A:      return INPUT_SELECT;   // A - confirm/play-pause
                case JOY_X:      return INPUT_BACK;     // X - back (B captured by system)
                case JOY_Y:      return INPUT_SHUFFLE;  // Y - shuffle toggle
                case JOY_L1:     return INPUT_PREV;     // L1 - previous track
                case JOY_R1:     return INPUT_NEXT;     // R1 - next track
                case JOY_SELECT: return INPUT_SHUFFLE;  // Select - also shuffle
                case JOY_START:  return INPUT_MENU;     // Start - menu
                case JOY_MENU:   return INPUT_MENU;     // Menu button
                default: break;
            }
            break;
        }

        case SDL_JOYBUTTONUP: {
            int btn = event->jbutton.button;
            if (btn < 16) g_button_state[btn] = 0;
            return INPUT_NONE;
        }

        case SDL_JOYHATMOTION:
            printf("[HAT] hat=%d value=%d\n", event->jhat.hat, event->jhat.value);
            fflush(stdout);
            switch (event->jhat.value) {
                case SDL_HAT_UP:    return INPUT_UP;
                case SDL_HAT_DOWN:  return INPUT_DOWN;
                case SDL_HAT_LEFT:  return INPUT_LEFT;
                case SDL_HAT_RIGHT: return INPUT_RIGHT;
                default: break;
            }
            break;

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
