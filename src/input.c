/**
 * Input Handler Implementation
 *
 * Maps keyboard/gamepad inputs to abstract actions.
 * Supports both desktop testing (keyboard) and Trimui Brick hardware.
 *
 * Uses SDL Game Controller API for standardized button mapping.
 * Falls back to raw joystick API if device has no mapping in gamecontrollerdb.txt.
 *
 * Game Controller Button Mapping (standardized):
 * - A: South button (confirm/select)
 * - B: East button (back) - captured by system on Trimui, use X as backup
 * - X: West button (backup back)
 * - Y: North button
 * - L/R: Shoulder buttons (prev/next track)
 * - Start: Options/menu
 * - Back/Select: Shuffle
 * - D-Pad: Navigation/seek/volume
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

// Gamepad button indices for raw joystick fallback (device-specific!)
// These are only used when SDL_GameController mapping is not available
#define BTN_A       0
#define BTN_B       1
#define BTN_L       4
#define BTN_R       5
#define BTN_SELECT  6
#define BTN_START   7

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

        // SDL Game Controller API - standardized button mapping
        case SDL_CONTROLLERBUTTONDOWN: {
            printf("[CTRL] button=%d (%s)\n", event->cbutton.button,
                   SDL_GameControllerGetStringForButton(event->cbutton.button));
            fflush(stdout);

            switch (event->cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A:             return INPUT_SELECT;   // A = confirm/play-pause
                case SDL_CONTROLLER_BUTTON_B:             return INPUT_BACK;     // B = back (may be captured by system)
                case SDL_CONTROLLER_BUTTON_X:             return INPUT_BACK;     // X = backup back
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return INPUT_PREV;     // L = previous track
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return INPUT_NEXT;     // R = next track
                case SDL_CONTROLLER_BUTTON_START:         return INPUT_MENU;     // Start = menu
                case SDL_CONTROLLER_BUTTON_BACK:          return INPUT_SHUFFLE;  // Select/Back = shuffle
                case SDL_CONTROLLER_BUTTON_DPAD_UP:       return INPUT_UP;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return INPUT_DOWN;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return INPUT_LEFT;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return INPUT_RIGHT;
                default: break;
            }
            break;
        }

        case SDL_CONTROLLERBUTTONUP:
            // Controller button released - no action needed
            return INPUT_NONE;

        // Raw Joystick API - fallback for devices without gamecontrollerdb mapping
        case SDL_JOYBUTTONDOWN: {
            int btn = event->jbutton.button;
            printf("[BTN] button=%d\n", btn);
            fflush(stdout);

            // Debouncing: ignore if button already pressed
            if (btn < 16 && g_button_state[btn]) {
                return INPUT_NONE;
            }
            if (btn < 16) g_button_state[btn] = 1;

            // FALLBACK: Raw joystick mapping (device-specific indices!)
            // Only used if Game Controller API mapping unavailable
            // TRIMUI Brick raw: A=0, X=1, Y=3, L=4, R=5, Select=6, Start=7
            switch (btn) {
                case 0:  return INPUT_SELECT;   // A button (south)
                case 1:  return INPUT_BACK;     // X button (west, since B is system)
                case 4:  return INPUT_PREV;     // L
                case 5:  return INPUT_NEXT;     // R
                case 7:  return INPUT_MENU;     // Start
                case 6:  return INPUT_SHUFFLE;  // Select
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
