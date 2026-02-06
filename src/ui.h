/**
 * UI Renderer - SDL2 based interface rendering
 */

#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <SDL2/SDL.h>

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
 * Render home menu screen (Resume, Browse, Favorites)
 */
void ui_render_home(void);

/**
 * Render resume list (tracks with saved positions)
 */
void ui_render_resume(void);

/**
 * Render favorites list
 */
void ui_render_favorites(void);

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

/**
 * Render equalizer screen (bass/treble horizontal bars)
 */
void ui_render_equalizer(void);

/**
 * Render help overlay for browser mode
 */
void ui_render_help_browser(void);

/**
 * Render help overlay for player mode
 */
void ui_render_help_player(void);

/**
 * Render loading screen
 * @param filename Name of file being loaded
 */
void ui_render_loading(const char *filename);

/**
 * Render error screen
 * @param message Error message to display
 */
void ui_render_error(const char *message);

/**
 * Render metadata scanning progress
 * @param current Current file number
 * @param total Total file count
 * @param current_file Name of file being scanned
 * @param found Number of files with metadata found
 */
void ui_render_scanning(int current, int total, const char *current_file, int found);

/**
 * Render scan complete screen
 * @param found Number of files with metadata found
 * @param total Total files scanned
 */
void ui_render_scan_complete(int found, int total);

/**
 * Render file options menu overlay
 */
void ui_render_file_menu(void);

/**
 * Render delete confirmation dialog
 */
void ui_render_confirm_delete(void);

/**
 * Render resume prompt dialog (ask user to resume from saved position)
 * @param saved_pos Saved position in seconds
 */
void ui_render_resume_prompt(int saved_pos);

/**
 * Render rename text input screen
 */
void ui_render_rename(void);

/**
 * Render YouTube search input screen
 */
void ui_render_youtube_search(void);

/**
 * Render YouTube search results list
 */
void ui_render_youtube_results(void);

/**
 * Render YouTube download progress
 */
void ui_render_youtube_download(void);

/**
 * Render download queue list view
 */
void ui_render_download_queue(void);

/**
 * Render Spotify Connect waiting screen
 */
void ui_render_spotify_connect(void);

/**
 * Render Spotify search input screen
 */
void ui_render_spotify_search(void);

/**
 * Render Spotify search results list
 */
void ui_render_spotify_results(void);

/**
 * Render Spotify now-playing screen (streaming)
 */
void ui_render_spotify_player(void);

/**
 * Render update screen (checking, available, downloading, ready)
 */
void ui_render_update(void);

/**
 * Show a toast notification
 * @param message Toast message text
 */
void ui_show_toast(const char *message);

/**
 * Check if toast should still be visible
 */
bool ui_toast_active(void);

/**
 * Get toast message (for external use)
 */
const char* ui_get_toast_message(void);

/**
 * Reset player title/artist scroll position
 * Call when changing songs to reset the scroll animation
 */
void ui_player_reset_scroll(void);

#endif // UI_H
