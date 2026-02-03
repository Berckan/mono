/**
 * Input Handler Implementation
 *
 * Maps keyboard/gamepad inputs to abstract actions.
 * Supports both desktop testing (keyboard) and Trimui Brick hardware.
 *
 * Trimui Brick Button Mapping (evdev):
 * - D-Pad: Standard axis/hat events
 * - A: South button (confirm)
 * - B: East button (back)
 * - X: West button
 * - Y: North button
 * - L/R: Shoulder buttons
 * - Start: Options/menu
 * - Select: Back/secondary
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

// Gamepad button indices (SDL standard)
#define BTN_A       0
#define BTN_B       1
#define BTN_L       4
#define BTN_R       5
#define BTN_SELECT  6
#define BTN_START   7

InputAction input_handle_event(const SDL_Event *event) {
    switch (event->type) {
        case SDL_KEYDOWN:
            // Ignore key repeats
            if (event->key.repeat) return INPUT_NONE;

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
                default: break;
            }
            break;

        case SDL_JOYBUTTONDOWN:
            switch (event->jbutton.button) {
                case BTN_A:      return INPUT_SELECT;
                case BTN_B:      return INPUT_BACK;
                case BTN_L:      return INPUT_PREV;
                case BTN_R:      return INPUT_NEXT;
                case BTN_START:  return INPUT_MENU;
                case BTN_SELECT: return INPUT_SHUFFLE;
                default: break;
            }
            break;

        case SDL_JOYHATMOTION:
            // D-Pad via hat
            switch (event->jhat.value) {
                case SDL_HAT_UP:    return INPUT_UP;
                case SDL_HAT_DOWN:  return INPUT_DOWN;
                case SDL_HAT_LEFT:  return INPUT_LEFT;
                case SDL_HAT_RIGHT: return INPUT_RIGHT;
                default: break;
            }
            break;

        case SDL_JOYAXISMOTION:
            // D-Pad via analog stick (for testing)
            // Axis 0 = X, Axis 1 = Y
            // Threshold to avoid drift
            if (abs(event->jaxis.value) > 16000) {
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
