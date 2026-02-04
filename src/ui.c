/**
 * UI Renderer Implementation
 *
 * Renders the minimalist interface. Screen dimensions auto-detected at runtime.
 * Uses SDL2 for rendering with a monochrome aesthetic.
 */

#include "ui.h"
#include "browser.h"
#include "audio.h"
#include "menu.h"
#include "favorites.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

// Screen dimensions (set by ui_init from auto-detection)
static int g_screen_width = 1280;   // Updated at runtime
static int g_screen_height = 720;   // Updated at runtime

// SDL objects
static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;

// Fonts (balanced for 720p)
static TTF_Font *g_font_large = NULL;   // 72px for titles
static TTF_Font *g_font_medium = NULL;  // 48px for text
static TTF_Font *g_font_small = NULL;   // 32px for details

// Colors (minimalist dark theme)
static const SDL_Color COLOR_BG = {18, 18, 18, 255};         // Near black
static const SDL_Color COLOR_TEXT = {240, 240, 240, 255};    // Off-white
static const SDL_Color COLOR_DIM = {128, 128, 128, 255};     // Gray
static const SDL_Color COLOR_ACCENT = {100, 200, 100, 255};  // Soft green
static const SDL_Color COLOR_HIGHLIGHT = {50, 50, 50, 255};  // Selection bg

// Layout constants (compact for 720p)
#define HEADER_HEIGHT 60
#define FOOTER_HEIGHT 50
#define MARGIN 20
#define LINE_HEIGHT 60

// Number of visible items in browser list
#define VISIBLE_ITEMS 9

/**
 * Load fonts from assets or system
 */
static int load_fonts(void) {
    // Try to load font from various locations
    const char *font_paths[] = {
        // Trimui Brick system fonts
        "/usr/trimui/res/regular.ttf",
        "/usr/trimui/res/full.ttf",
        // Pak-bundled font
        "Mono.pak/assets/fonts/mono.ttf",
        "./assets/fonts/mono.ttf",
        // Linux system fonts
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        // macOS (for desktop testing)
        "/System/Library/Fonts/Monaco.ttf",
        "/System/Library/Fonts/Supplemental/Menlo.ttc",
        NULL
    };

    const char *found_path = NULL;
    for (int i = 0; font_paths[i]; i++) {
        FILE *f = fopen(font_paths[i], "rb");
        if (f) {
            fclose(f);
            found_path = font_paths[i];
            break;
        }
    }

    if (!found_path) {
        fprintf(stderr, "No suitable font found\n");
        return -1;
    }

    printf("Using font: %s\n", found_path);

    g_font_large = TTF_OpenFont(found_path, 72);
    g_font_medium = TTF_OpenFont(found_path, 48);
    g_font_small = TTF_OpenFont(found_path, 32);

    if (!g_font_large || !g_font_medium || !g_font_small) {
        fprintf(stderr, "Failed to load fonts: %s\n", TTF_GetError());
        return -1;
    }

    // Use light hinting for smoother text on small LCD screens
    // TTF_HINTING_LIGHT produces better results than MONO for 320x240
    TTF_SetFontHinting(g_font_large, TTF_HINTING_LIGHT);
    TTF_SetFontHinting(g_font_medium, TTF_HINTING_LIGHT);
    TTF_SetFontHinting(g_font_small, TTF_HINTING_LIGHT);

    return 0;
}

/**
 * Render text to screen
 */
static void render_text(const char *text, int x, int y, TTF_Font *font, SDL_Color color) {
    if (!text || !text[0]) return;

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, surface);
    if (texture) {
        SDL_Rect dest = {x, y, surface->w, surface->h};
        SDL_RenderCopy(g_renderer, texture, NULL, &dest);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

/**
 * Render text truncated to max width
 */
static void render_text_truncated(const char *text, int x, int y, int max_width,
                                   TTF_Font *font, SDL_Color color) {
    if (!text || !text[0]) return;

    char truncated[256];
    strncpy(truncated, text, sizeof(truncated) - 1);
    truncated[sizeof(truncated) - 1] = '\0';

    int w, h;
    TTF_SizeUTF8(font, truncated, &w, &h);

    // Truncate with ellipsis if too long
    while (w > max_width && strlen(truncated) > 3) {
        truncated[strlen(truncated) - 1] = '\0';
        truncated[strlen(truncated) - 1] = '.';
        truncated[strlen(truncated) - 2] = '.';
        truncated[strlen(truncated) - 3] = '.';
        TTF_SizeUTF8(font, truncated, &w, &h);
    }

    render_text(truncated, x, y, font, color);
}

/**
 * Render centered text
 */
static void render_text_centered(const char *text, int y, TTF_Font *font, SDL_Color color) {
    if (!text || !text[0]) return;

    int w, h;
    TTF_SizeUTF8(font, text, &w, &h);
    int x = (g_screen_width - w) / 2;
    render_text(text, x, y, font, color);
}

/**
 * Draw filled rectangle
 */
static void draw_rect(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(g_renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(g_renderer, &rect);
}

/**
 * Format seconds as MM:SS
 */
static void format_time(int seconds, char *buffer, size_t size) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    snprintf(buffer, size, "%d:%02d", mins, secs);
}

int ui_init(int width, int height) {
    g_screen_width = width;
    g_screen_height = height;

    // CRITICAL: Set hints BEFORE creating window/renderer
    // This ensures linear filtering is applied to all textures
    SDL_SetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER, "1");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");  // Linear filtering
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");       // Batch render calls

    // Create fullscreen window for embedded device (true fullscreen, no letterbox)
    g_window = SDL_CreateWindow(
        "Mono",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN
    );

    if (!g_window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        return -1;
    }

    // Create renderer
    g_renderer = SDL_CreateRenderer(g_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!g_renderer) {
        // Fallback to software renderer
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!g_renderer) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        return -1;
    }

    // No logical size - render at native resolution without letterboxing

    // Load fonts
    if (load_fonts() < 0) {
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        return -1;
    }

    return 0;
}

void ui_cleanup(void) {
    if (g_font_large) TTF_CloseFont(g_font_large);
    if (g_font_medium) TTF_CloseFont(g_font_medium);
    if (g_font_small) TTF_CloseFont(g_font_small);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
}

void ui_render_browser(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_text("> Mono", MARGIN, 8, g_font_medium, COLOR_TEXT);

    // Draw header separator
    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // File list
    int count = browser_get_count();
    int cursor = browser_get_cursor();
    int scroll = browser_get_scroll_offset();

    int y = HEADER_HEIGHT + 8;
    int max_width = g_screen_width - MARGIN * 4;

    for (int i = 0; i < VISIBLE_ITEMS && (scroll + i) < count; i++) {
        const FileEntry *entry = browser_get_entry(scroll + i);
        if (!entry) continue;

        bool is_selected = (scroll + i) == cursor;
        bool is_favorite = (entry->type == ENTRY_FILE) && favorites_is_favorite(entry->full_path);

        // Selection highlight
        if (is_selected) {
            draw_rect(0, y - 6, g_screen_width, LINE_HEIGHT, COLOR_HIGHLIGHT);
        }

        // Icon prefix (ASCII-safe for embedded fonts)
        // Use * for favorites since embedded fonts may not have Unicode stars
        const char *prefix;
        if (entry->type == ENTRY_DIRECTORY) {
            prefix = "[DIR] ";
        } else if (is_favorite) {
            prefix = "* ";  // Favorite indicator
        } else {
            prefix = "> ";
        }

        // Build display text
        char display[300];
        snprintf(display, sizeof(display), "%s%s", prefix, entry->name);

        // Render with appropriate color (favorites get accent color)
        SDL_Color color;
        if (is_selected) {
            color = COLOR_ACCENT;
        } else if (is_favorite) {
            color = COLOR_ACCENT;  // Favorites shown in accent color
        } else {
            color = COLOR_TEXT;
        }
        render_text_truncated(display, MARGIN, y, max_width, g_font_medium, color);

        y += LINE_HEIGHT;
    }

    // Footer with scroll indicator
    if (count > VISIBLE_ITEMS) {
        draw_rect(0, g_screen_height - FOOTER_HEIGHT, g_screen_width, 2, COLOR_DIM);

        char scroll_info[32];
        snprintf(scroll_info, sizeof(scroll_info), "%d/%d", cursor + 1, count);
        render_text(scroll_info, g_screen_width - 120, g_screen_height - 40, g_font_small, COLOR_DIM);
    }

    // Empty directory message
    if (count == 0) {
        render_text_centered("No music files found", g_screen_height / 2, g_font_medium, COLOR_DIM);
    }

    // Controls hint
    render_text("A:Select  Y:Fav  X:Back", MARGIN, g_screen_height - 40, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

// Internal: renders player content without SDL_RenderPresent
static void ui_render_player_content(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    const TrackInfo *info = audio_get_track_info();

    // Header
    render_text("> Mono", MARGIN, 8, g_font_medium, COLOR_TEXT);

    // Favorite indicator if current track is a favorite
    const char *current_path = browser_get_selected_path();
    if (current_path && favorites_is_favorite(current_path)) {
        render_text("*", 180, 8, g_font_medium, COLOR_ACCENT);
    }

    // Play state icon (ASCII-safe)
    const char *state_icon = audio_is_playing() ? "PLAY" : (audio_is_paused() ? "PAUS" : "STOP");
    render_text(state_icon, g_screen_width - 120, 8, g_font_medium, COLOR_ACCENT);

    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // Now Playing section (centered)
    int center_y = 150;

    // Title (large, centered)
    render_text_centered(info->title, center_y, g_font_large, COLOR_TEXT);

    // Artist (medium, centered, dimmer)
    render_text_centered(info->artist, center_y + 90, g_font_medium, COLOR_DIM);

    // Progress bar
    int bar_y = center_y + 200;
    int bar_x = MARGIN * 2;
    int bar_w = g_screen_width - MARGIN * 4;
    int bar_h = 12;

    // Background bar
    draw_rect(bar_x, bar_y, bar_w, bar_h, COLOR_HIGHLIGHT);

    // Progress fill
    if (info->duration_sec > 0) {
        float progress = (float)info->position_sec / info->duration_sec;
        int fill_w = (int)(bar_w * progress);
        draw_rect(bar_x, bar_y, fill_w, bar_h, COLOR_ACCENT);
    }

    // Time display
    char time_str[32];
    char pos_str[16], dur_str[16];
    format_time(info->position_sec, pos_str, sizeof(pos_str));

    // Show "--:--" if duration is unknown (Mix_MusicDuration not available)
    if (info->duration_sec > 0) {
        format_time(info->duration_sec, dur_str, sizeof(dur_str));
    } else {
        snprintf(dur_str, sizeof(dur_str), "--:--");
    }
    snprintf(time_str, sizeof(time_str), "%s / %s", pos_str, dur_str);

    int tw, th;
    TTF_SizeUTF8(g_font_small, time_str, &tw, &th);
    render_text(time_str, (g_screen_width - tw) / 2, bar_y + 24, g_font_small, COLOR_DIM);

    // Footer with controls
    draw_rect(0, g_screen_height - FOOTER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // Control icons (ASCII-safe)
    int ctrl_y = g_screen_height - 40;
    render_text("L:Prev", MARGIN, ctrl_y, g_font_small, COLOR_DIM);
    render_text("A:Play", 150, ctrl_y, g_font_small, COLOR_DIM);
    render_text("R:Next", 300, ctrl_y, g_font_small, COLOR_DIM);

    // Volume indicator
    char vol_str[16];
    snprintf(vol_str, sizeof(vol_str), "Vol:%d%%", audio_get_volume());
    render_text(vol_str, 450, ctrl_y, g_font_small, COLOR_DIM);

    render_text("X:Back", g_screen_width - 140, ctrl_y, g_font_small, COLOR_DIM);
}

void ui_render_player(void) {
    ui_render_player_content();
    SDL_RenderPresent(g_renderer);
}

void ui_render_menu(void) {
    // Render player underneath (without presenting yet)
    ui_render_player_content();

    // Darken overlay
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 180);
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);

    // Menu box (compact for 720p)
    int menu_w = 500;
    int menu_h = 320;
    int menu_x = (g_screen_width - menu_w) / 2;
    int menu_y = (g_screen_height - menu_h) / 2;

    draw_rect(menu_x, menu_y, menu_w, menu_h, COLOR_HIGHLIGHT);

    // Menu title
    render_text_centered("Options", menu_y + 16, g_font_medium, COLOR_TEXT);

    // Menu items
    int cursor = menu_get_cursor();
    int item_y = menu_y + 70;
    int item_h = 50;
    char buf[64];

    // Shuffle
    snprintf(buf, sizeof(buf), "Shuffle: %s", menu_is_shuffle_enabled() ? "On" : "Off");
    if (cursor == MENU_SHUFFLE) {
        draw_rect(menu_x + 16, item_y - 4, menu_w - 32, item_h, COLOR_ACCENT);
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_BG);
    } else {
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_DIM);
    }
    item_y += item_h;

    // Repeat
    snprintf(buf, sizeof(buf), "Repeat: %s", menu_get_repeat_string());
    if (cursor == MENU_REPEAT) {
        draw_rect(menu_x + 16, item_y - 4, menu_w - 32, item_h, COLOR_ACCENT);
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_BG);
    } else {
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_DIM);
    }
    item_y += item_h;

    // Sleep Timer
    snprintf(buf, sizeof(buf), "Sleep: %s", menu_get_sleep_string());
    if (cursor == MENU_SLEEP) {
        draw_rect(menu_x + 16, item_y - 4, menu_w - 32, item_h, COLOR_ACCENT);
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_BG);
    } else {
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_DIM);
    }
    item_y += item_h;

    // Exit
    if (cursor == MENU_EXIT) {
        draw_rect(menu_x + 16, item_y - 4, menu_w - 32, item_h, COLOR_ACCENT);
        render_text("Exit to Browser", menu_x + 32, item_y, g_font_small, COLOR_BG);
    } else {
        render_text("Exit to Browser", menu_x + 32, item_y, g_font_small, COLOR_DIM);
    }

    // Controls hint
    render_text_centered("A:Select  X:Close", menu_y + menu_h - 40, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

/**
 * Internal: render help overlay content
 */
static void render_help_overlay(const char *title, const char *lines[], int line_count) {
    // Darken overlay
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 200);
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);

    // Help box (centered)
    int box_w = 600;
    int box_h = 60 + (line_count * 45);
    int box_x = (g_screen_width - box_w) / 2;
    int box_y = (g_screen_height - box_h) / 2;

    draw_rect(box_x, box_y, box_w, box_h, COLOR_HIGHLIGHT);

    // Title
    render_text_centered(title, box_y + 16, g_font_medium, COLOR_ACCENT);

    // Control lines
    int line_y = box_y + 70;
    for (int i = 0; i < line_count; i++) {
        render_text(lines[i], box_x + 40, line_y, g_font_small, COLOR_TEXT);
        line_y += 45;
    }

    // Dismiss hint
    render_text_centered("Release Y to close", box_y + box_h - 35, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

void ui_render_help_browser(void) {
    // First render browser underneath
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);
    // Simplified background, then overlay

    const char *lines[] = {
        "D-Pad      Navigate files",
        "A          Open folder / Play file",
        "X          Go up / Exit at root",
        "Y (tap)    Toggle favorite",
        "Y (hold)   Show this help",
        "Start      (in player) Options menu"
    };

    render_help_overlay("Browser Controls", lines, 6);
}

void ui_render_help_player(void) {
    // Render player content underneath
    ui_render_player_content();

    const char *lines[] = {
        "A          Play / Pause",
        "L / R      Previous / Next track",
        "D-Pad L/R  Seek -/+ 10 seconds",
        "D-Pad U/D  Volume up / down",
        "Y (tap)    Toggle favorite",
        "Select     Dim screen (battery save)",
        "Start      Options menu",
        "X          Back to browser"
    };

    render_help_overlay("Player Controls", lines, 8);
}
