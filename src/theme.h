/**
 * Theme System - Color themes for the UI
 */

#ifndef THEME_H
#define THEME_H

#include <SDL2/SDL.h>

/**
 * Available themes
 */
typedef enum {
    THEME_DARK,
    THEME_LIGHT,
    THEME_COUNT
} ThemeId;

/**
 * Theme color palette
 */
typedef struct {
    SDL_Color bg;        // Background color
    SDL_Color text;      // Primary text color
    SDL_Color dim;       // Dimmed/secondary text
    SDL_Color accent;    // Accent color (highlight selections, etc.)
    SDL_Color highlight; // Selection background
    SDL_Color error;     // Error/warning color
} ThemeColors;

/**
 * Initialize theme system
 */
void theme_init(void);

/**
 * Get current theme ID
 */
ThemeId theme_get_current(void);

/**
 * Set current theme
 */
void theme_set(ThemeId id);

/**
 * Cycle to next theme
 */
void theme_cycle(void);

/**
 * Get current theme colors
 */
const ThemeColors* theme_get_colors(void);

/**
 * Get theme name string
 */
const char* theme_get_name(ThemeId id);

/**
 * Get current theme name
 */
const char* theme_get_current_name(void);

#endif // THEME_H
