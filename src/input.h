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
    INPUT_BACK,      // X button (B is captured by system)
    INPUT_PREV,      // L button
    INPUT_NEXT,      // R button
    INPUT_MENU,      // Start button
    INPUT_SHUFFLE,   // Select button (browser: shuffle, player: dim screen)
    INPUT_FAVORITE,  // Y button tap - toggle favorite
    INPUT_HELP,      // Y button hold - show help overlay
    INPUT_VOL_UP,    // Hardware volume up
    INPUT_VOL_DOWN   // Hardware volume down
} InputAction;

/**
 * Handle SDL event and return corresponding action
 * @param event SDL event to process
 * @return Input action or INPUT_NONE
 */
InputAction input_handle_event(const SDL_Event *event);

/**
 * Poll for hold actions (call once per frame)
 * Returns INPUT_HELP if Y button is held for threshold time
 * @return Input action or INPUT_NONE
 */
InputAction input_poll_holds(void);

#endif // INPUT_H
