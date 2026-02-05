/**
 * Cover Art - Album cover display for music player
 *
 * Loads and displays album cover images (cover.png/jpg) from
 * the directory containing the current track.
 */

#ifndef COVER_H
#define COVER_H

#include <SDL2/SDL.h>
#include <stdbool.h>

/**
 * Initialize cover art system
 * @param renderer SDL renderer to use for textures
 * @return 0 on success, -1 on failure
 */
int cover_init(SDL_Renderer *renderer);

/**
 * Cleanup cover art resources
 */
void cover_cleanup(void);

/**
 * Load cover art from directory
 * Searches for cover.png, cover.jpg, folder.*, album.*, front.*
 * @param dir_path Directory to search for cover art
 * @return true if cover was found and loaded
 */
bool cover_load(const char *dir_path);

/**
 * Get current cover art texture
 * @return SDL_Texture pointer, or NULL if no cover loaded
 */
SDL_Texture* cover_get_texture(void);

/**
 * Get cover art dimensions (scaled to fit display)
 * @param w Output width
 * @param h Output height
 */
void cover_get_size(int *w, int *h);

/**
 * Check if cover art is loaded
 * @return true if cover is available
 */
bool cover_is_loaded(void);

/**
 * Clear current cover (e.g., when changing directories)
 */
void cover_clear(void);

/**
 * Check if current cover is predominantly dark
 * Used for adaptive text colors in player view
 * @return true if cover is dark (use light text), false if light (use dark text)
 */
bool cover_is_dark(void);

#endif // COVER_H
