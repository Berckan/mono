/**
 * UI Renderer - SDL2 based interface rendering
 */

#ifndef UI_H
#define UI_H

/**
 * Initialize UI system
 * @param width Screen width
 * @param height Screen height
 * @return 0 on success, -1 on failure
 */
int ui_init(int width, int height);

/**
 * Cleanup UI resources
 */
void ui_cleanup(void);

/**
 * Render file browser screen
 */
void ui_render_browser(void);

/**
 * Render now playing screen
 */
void ui_render_player(void);

/**
 * Render options menu overlay
 */
void ui_render_menu(void);

#endif // UI_H
