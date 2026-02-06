/**
 * Input Handler - D-Pad and button mapping for Trimui Brick
 */

#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>
#include <stdbool.h>

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
    INPUT_PREV,      // L1 button
    INPUT_NEXT,      // R1 button
    INPUT_SEEK_START,// L2 button - jump to start of track
    INPUT_SEEK_END,  // R2 button - jump to end of track
    INPUT_MENU,      // Start button
    INPUT_SHUFFLE,   // Select button (browser: shuffle, player: dim screen)
    INPUT_FAVORITE,  // Y button tap - toggle favorite
    INPUT_HELP,      // Y button hold - show help overlay
    INPUT_EXIT,      // Start + B combo - exit app
    INPUT_VOL_UP,    // Hardware volume up
    INPUT_VOL_DOWN,  // Hardware volume down
    INPUT_SUSPEND    // Power button - system suspend
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

/**
 * Get seek amount if Left/Right D-pad is being held
 * Returns 0 if not held, or seek amount in seconds (negative for left)
 * Amount accelerates the longer the button is held:
 * - 0-1s: 5 seconds per call
 * - 1-2s: 15 seconds per call
 * - 2-3s: 30 seconds per call
 * - 3s+:  60 seconds per call
 * Call this at regular intervals (e.g., every 100ms) while in player state
 */
int input_get_seek_amount(void);

/**
 * Check if seeking is active (Left or Right held)
 */
bool input_is_seeking(void);

/**
 * Initialize input system (opens power button device on Linux)
 * @return true on success
 */
bool input_init(void);

/**
 * Cleanup input system
 */
void input_cleanup(void);

/**
 * Poll power button (reads from /dev/input/event1 on Linux)
 * @return INPUT_SUSPEND if power pressed, INPUT_NONE otherwise
 */
InputAction input_poll_power(void);

/**
 * Drain power button buffer (call after waking from suspend)
 * Clears any accumulated events to prevent immediate re-suspend
 */
void input_drain_power(void);

#endif // INPUT_H
