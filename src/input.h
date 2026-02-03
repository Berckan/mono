/**
 * Input Handler - D-Pad and button mapping for Trimui Brick
 */

#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>

/**
 * Input actions abstracted from hardware
 */
typedef enum {
    INPUT_NONE,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_SELECT,    // A button
    INPUT_BACK,      // B button
    INPUT_PREV,      // L button
    INPUT_NEXT,      // R button
    INPUT_MENU,      // Start button
    INPUT_SHUFFLE    // Select button
} InputAction;

/**
 * Handle SDL event and return corresponding action
 * @param event SDL event to process
 * @return Input action or INPUT_NONE
 */
InputAction input_handle_event(const SDL_Event *event);

#endif // INPUT_H
