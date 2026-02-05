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
#include "sysinfo.h"
#include "positions.h"
#include "cover.h"
#include "filemenu.h"
#include "metadata.h"
#include "theme.h"
#include "ytsearch.h"
#include "youtube.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

// Theme color accessors (macros for cleaner code)
#define COLOR_BG (theme_get_colors()->bg)
#define COLOR_TEXT (theme_get_colors()->text)
#define COLOR_DIM (theme_get_colors()->dim)
#define COLOR_ACCENT (theme_get_colors()->accent)
#define COLOR_HIGHLIGHT (theme_get_colors()->highlight)
#define COLOR_ERROR (theme_get_colors()->error)

// Layout constants (compact for 720p)
#define HEADER_HEIGHT 60
#define FOOTER_HEIGHT 50
#define MARGIN 20
#define LINE_HEIGHT 60

// Number of visible items in browser list
#define VISIBLE_ITEMS 9

// Dancing monkey animation (16x16 pixels, 4 frames)
// 0 = transparent, 1 = brown (fur), 2 = beige (face/belly), 3 = black (eyes/nose/mouth)
// Cute monkey inspired by classic pixel art style
static const uint8_t MONKEY_FRAMES[4][16][16] = {
    // Frame 0: Standing, arms down
    {
        {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,1,1,1,2,2,2,2,2,1,1,1,0,0,0},
        {0,1,1,1,2,2,2,2,2,2,2,1,1,1,0,0},
        {0,1,1,2,2,3,2,2,2,3,2,2,1,1,0,0},
        {0,1,1,2,2,3,2,2,2,3,2,2,1,1,0,0},
        {0,1,1,2,2,2,2,3,2,2,2,2,1,1,0,0},
        {0,0,1,1,2,2,3,3,3,2,2,1,1,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,1,1,0,1,2,2,2,1,0,1,1,0,0,0},
        {0,0,1,1,0,1,2,2,2,1,0,1,1,0,0,0},
        {0,0,0,0,0,1,2,2,2,1,0,0,0,1,0,0},
        {0,0,0,0,0,1,1,0,1,1,0,0,1,1,0,0},
        {0,0,0,0,0,1,1,0,1,1,0,1,1,0,0,0},
        {0,0,0,0,1,1,0,0,0,1,1,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    },
    // Frame 1: Left arm up, dancing
    {
        {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,1,1,1,2,2,2,2,2,1,1,1,0,0,0},
        {0,1,1,1,2,2,2,2,2,2,2,1,1,1,0,0},
        {0,1,1,2,2,3,2,2,2,3,2,2,1,1,0,0},
        {0,1,1,2,2,3,2,2,2,3,2,2,1,1,0,0},
        {0,1,1,2,2,2,2,3,2,2,2,2,1,1,0,0},
        {0,0,1,1,2,2,3,3,3,2,2,1,1,0,0,0},
        {0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {1,1,0,0,0,1,2,2,2,1,0,1,1,0,0,0},
        {0,0,0,0,0,1,2,2,2,1,0,1,1,0,0,0},
        {0,0,0,0,0,1,2,2,2,1,0,0,0,0,1,0},
        {0,0,0,0,0,1,1,0,1,1,0,0,0,1,1,0},
        {0,0,0,0,0,1,1,0,1,1,0,0,1,1,0,0},
        {0,0,0,0,1,1,0,0,0,1,1,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    },
    // Frame 2: Both arms up, happy!
    {
        {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,1,1,1,2,2,2,2,2,1,1,1,0,0,0},
        {0,1,1,1,2,2,2,2,2,2,2,1,1,1,0,0},
        {0,1,1,2,2,3,2,2,2,3,2,2,1,1,0,0},
        {0,1,1,2,2,3,2,2,2,3,2,2,1,1,0,0},
        {0,1,1,2,2,2,3,3,3,2,2,2,1,1,0,0},
        {0,0,1,1,2,2,2,3,2,2,2,1,1,0,0,0},
        {0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
        {1,1,0,0,0,1,2,2,2,1,0,0,0,1,1,0},
        {0,0,0,0,0,1,2,2,2,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,2,2,2,1,0,0,0,1,0,0},
        {0,0,0,0,0,1,1,0,1,1,0,0,1,0,0,0},
        {0,0,0,0,0,1,1,0,1,1,0,0,1,0,0,0},
        {0,0,0,0,1,1,0,0,0,1,1,0,0,1,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0},
    },
    // Frame 3: Right arm up, dancing
    {
        {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,1,1,1,2,2,2,2,2,1,1,1,0,0,0},
        {0,1,1,1,2,2,2,2,2,2,2,1,1,1,0,0},
        {0,1,1,2,2,3,2,2,2,3,2,2,1,1,0,0},
        {0,1,1,2,2,3,2,2,2,3,2,2,1,1,0,0},
        {0,1,1,2,2,2,2,3,2,2,2,2,1,1,0,0},
        {0,0,1,1,2,2,3,3,3,2,2,1,1,1,1,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0},
        {0,0,1,1,0,1,2,2,2,1,0,0,0,0,0,0},
        {0,0,1,1,0,1,2,2,2,1,0,0,0,0,0,0},
        {0,1,0,0,0,1,2,2,2,1,0,0,0,0,0,0},
        {0,1,1,0,0,1,1,0,1,1,0,0,0,0,0,0},
        {0,0,1,1,0,1,1,0,1,1,0,0,0,0,0,0},
        {0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    },
};

// Animation state
static int g_monkey_frame = 0;
static Uint32 g_monkey_last_update = 0;
#define MONKEY_FRAME_MS 200  // ms per frame
#define MONKEY_PIXEL_SIZE 3  // scale factor

// Monkey colors
static const SDL_Color COLOR_MONKEY_BROWN = {139, 90, 43, 255};   // Brown fur
static const SDL_Color COLOR_MONKEY_BEIGE = {222, 184, 135, 255}; // Beige face/belly
static const SDL_Color COLOR_MONKEY_BLACK = {30, 30, 30, 255};    // Black eyes/nose

/**
 * Render dancing monkey sprite
 */
static void render_monkey(int x, int y, bool is_playing) {
    Uint32 now = SDL_GetTicks();

    // Update animation frame if playing
    if (is_playing) {
        if (now - g_monkey_last_update > MONKEY_FRAME_MS) {
            g_monkey_frame = (g_monkey_frame + 1) % 4;
            g_monkey_last_update = now;
        }
    } else {
        g_monkey_frame = 0;  // Static pose when not playing
    }

    const uint8_t (*frame)[16] = MONKEY_FRAMES[g_monkey_frame];

    for (int py = 0; py < 16; py++) {
        for (int px = 0; px < 16; px++) {
            uint8_t pixel = frame[py][px];
            if (pixel == 0) continue;  // Transparent

            SDL_Color color;
            switch (pixel) {
                case 1: color = COLOR_MONKEY_BROWN; break;  // Fur
                case 2: color = COLOR_MONKEY_BEIGE; break;  // Face/belly
                case 3: color = COLOR_MONKEY_BLACK; break;  // Eyes/nose/mouth
                default: continue;
            }

            SDL_SetRenderDrawColor(g_renderer, color.r, color.g, color.b, 255);
            SDL_Rect rect = {
                x + px * MONKEY_PIXEL_SIZE,
                y + py * MONKEY_PIXEL_SIZE,
                MONKEY_PIXEL_SIZE,
                MONKEY_PIXEL_SIZE
            };
            SDL_RenderFillRect(g_renderer, &rect);
        }
    }
}

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

    // Use no hinting for pixel-art retro look
    // TTF_HINTING_NONE produces sharp, blocky text
    TTF_SetFontHinting(g_font_large, TTF_HINTING_NONE);
    TTF_SetFontHinting(g_font_medium, TTF_HINTING_NONE);
    TTF_SetFontHinting(g_font_small, TTF_HINTING_NONE);

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
 * Render text with shadow (for cover background readability)
 */
static void render_text_shadow(const char *text, int x, int y, TTF_Font *font, SDL_Color color) {
    if (!text || !text[0]) return;

    // Shadow color (black with transparency for soft effect)
    SDL_Color shadow = {0, 0, 0, 180};

    // Render shadow (offset by 2px, with blur effect using multiple passes)
    SDL_Surface *shadow_surface = TTF_RenderUTF8_Blended(font, text, shadow);
    if (shadow_surface) {
        SDL_Texture *shadow_tex = SDL_CreateTextureFromSurface(g_renderer, shadow_surface);
        if (shadow_tex) {
            SDL_SetTextureBlendMode(shadow_tex, SDL_BLENDMODE_BLEND);
            // Multiple shadow passes for blur effect
            SDL_Rect dest1 = {x + 1, y + 1, shadow_surface->w, shadow_surface->h};
            SDL_Rect dest2 = {x + 2, y + 2, shadow_surface->w, shadow_surface->h};
            SDL_SetTextureAlphaMod(shadow_tex, 100);
            SDL_RenderCopy(g_renderer, shadow_tex, NULL, &dest2);
            SDL_SetTextureAlphaMod(shadow_tex, 150);
            SDL_RenderCopy(g_renderer, shadow_tex, NULL, &dest1);
            SDL_DestroyTexture(shadow_tex);
        }
        SDL_FreeSurface(shadow_surface);
    }

    // Render main text
    render_text(text, x, y, font, color);
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
 * Render centered text with shadow
 */
static void render_text_centered_shadow(const char *text, int y, TTF_Font *font, SDL_Color color) {
    if (!text || !text[0]) return;

    int w, h;
    TTF_SizeUTF8(font, text, &w, &h);
    int x = (g_screen_width - w) / 2;
    render_text_shadow(text, x, y, font, color);
}

/**
 * Draw a filled rect with a thick retro border
 */
static void draw_retro_box(int x, int y, int w, int h, int border_thick, SDL_Color fill, SDL_Color border) {
    // Fill
    SDL_SetRenderDrawColor(g_renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(g_renderer, &rect);

    // Border (draw multiple rects for thickness)
    SDL_SetRenderDrawColor(g_renderer, border.r, border.g, border.b, border.a);
    for (int i = 0; i < border_thick; i++) {
        SDL_Rect b = {x + i, y + i, w - i * 2, h - i * 2};
        SDL_RenderDrawRect(g_renderer, &b);
    }
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

/**
 * Render horizontal battery icon with fill level
 * @param x X position
 * @param y Y position
 * @param percent Battery percentage (0-100)
 * @param charging True if charging
 */
static void render_battery_icon(int x, int y, int percent, bool charging) {
    // Battery dimensions
    int batt_w = 32;
    int batt_h = 16;
    int tip_w = 4;
    int tip_h = 8;
    int border = 2;

    // Battery outline
    SDL_SetRenderDrawColor(g_renderer, COLOR_TEXT.r, COLOR_TEXT.g, COLOR_TEXT.b, 255);

    // Main body outline
    SDL_Rect outline = {x, y, batt_w, batt_h};
    SDL_RenderDrawRect(g_renderer, &outline);

    // Positive terminal (tip on right)
    SDL_Rect tip = {x + batt_w, y + (batt_h - tip_h) / 2, tip_w, tip_h};
    SDL_RenderFillRect(g_renderer, &tip);

    // Fill based on percentage
    if (percent > 0) {
        int fill_w = ((batt_w - border * 2) * percent) / 100;
        SDL_Rect fill = {x + border, y + border, fill_w, batt_h - border * 2};

        // Color based on level
        SDL_Color fill_color;
        if (charging) {
            fill_color = COLOR_ACCENT;  // Green when charging
        } else if (percent <= 20) {
            fill_color = COLOR_ERROR;  // Retro Red
        } else if (percent <= 40) {
            fill_color = (SDL_Color){255, 160, 0, 255}; // Retro Orange
        } else {
            fill_color = COLOR_DIM;  // Light Blue-Gray
        }

        SDL_SetRenderDrawColor(g_renderer, fill_color.r, fill_color.g, fill_color.b, 255);
        SDL_RenderFillRect(g_renderer, &fill);
    }

    // Charging indicator (above battery, centered)
    if (charging) {
        render_text("⚡", x + batt_w / 2 - 8, y - 22, g_font_small, COLOR_ACCENT);
    }
}

/**
 * Render play/pause icon centered at position
 * When playing: shows pause icon (two vertical bars)
 * When paused: shows play icon (triangle)
 */
static void render_play_pause_icon(int center_x, int y, bool is_playing) {
    int icon_size = 40;  // Overall size

    SDL_SetRenderDrawColor(g_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);

    if (is_playing) {
        // Pause icon: two vertical bars
        int bar_w = 10;
        int bar_h = icon_size;
        int gap = 8;

        SDL_Rect left_bar = {center_x - gap - bar_w, y, bar_w, bar_h};
        SDL_Rect right_bar = {center_x + gap, y, bar_w, bar_h};

        SDL_RenderFillRect(g_renderer, &left_bar);
        SDL_RenderFillRect(g_renderer, &right_bar);
    } else {
        // Play icon: triangle pointing right (drawn as filled polygon approximation)
        int tri_w = icon_size - 5;
        int tri_h = icon_size;
        int start_x = center_x - tri_w / 2;

        // Draw triangle using horizontal lines (filled)
        for (int row = 0; row < tri_h; row++) {
            // Calculate width at this row (narrower at top and bottom)
            int half_height = tri_h / 2;
            int dist_from_center = abs(row - half_height);
            int line_width = tri_w - (dist_from_center * tri_w / half_height);
            if (line_width > 0) {
                SDL_RenderDrawLine(g_renderer, start_x, y + row, start_x + line_width, y + row);
            }
        }
    }
}

/**
 * Render status bar (volume and battery) in top right corner
 * Call this from both browser and player renders
 */
static void render_status_bar(void) {
    int text_y = 16;  // Vertical position for text
    int batt_y = 24;  // Battery icon vertically centered with text (32px font, 16px icon)
    int right_margin = 20;  // Keep away from screen edge

    // Battery indicator (rightmost)
    int batt_pct = sysinfo_get_battery_percent();
    bool charging = sysinfo_is_charging();

    if (batt_pct >= 0) {
        // Battery percentage text (white)
        char batt_str[16];
        snprintf(batt_str, sizeof(batt_str), "%d%%", batt_pct);
        render_text(batt_str, g_screen_width - 70 - right_margin, text_y, g_font_small, COLOR_TEXT);

        // Battery icon (to the left of text, vertically centered)
        render_battery_icon(g_screen_width - 115 - right_margin, batt_y, batt_pct, charging);
    }

    // Volume indicator (to the left of battery, with more spacing) - white text
    char vol_str[16];
    int sys_vol = sysinfo_get_volume();
    if (sys_vol >= 0) {
        snprintf(vol_str, sizeof(vol_str), "Vol:%d%%", sys_vol);
    } else {
        snprintf(vol_str, sizeof(vol_str), "Vol:--");
    }
    render_text(vol_str, g_screen_width - 280 - right_margin, text_y, g_font_small, COLOR_TEXT);
}

int ui_init(int width, int height) {
    g_screen_width = width;
    g_screen_height = height;

    // CRITICAL: Set hints BEFORE creating window/renderer
    // This ensures linear filtering is applied to all textures
    SDL_SetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER, "1");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");  // Linear filtering
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");       // Batch render calls

#ifdef __APPLE__
    // macOS: windowed mode at Trimui Brick resolution for desktop testing
    g_window = SDL_CreateWindow(
        "Mono - Desktop Preview (1280x720)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN
    );
#else
    // Embedded device: fullscreen
    g_window = SDL_CreateWindow(
        "Mono",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN
    );
#endif

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

    // Initialize cover art system
    cover_init(g_renderer);

    return 0;
}

void ui_cleanup(void) {
    cover_cleanup();
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

    // Status bar (volume + battery) in top right
    render_status_bar();

    // Draw header separator
    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // File list
    int count = browser_get_count();
    int cursor = browser_get_cursor();
    int scroll = browser_get_scroll_offset();

    int y = HEADER_HEIGHT + 8;
    int max_width = g_screen_width - MARGIN * 4;

    // Color for played files (orange/amber)
    SDL_Color COLOR_PLAYED = {255, 160, 0, 255};

    for (int i = 0; i < VISIBLE_ITEMS && (scroll + i) < count; i++) {
        const FileEntry *entry = browser_get_entry(scroll + i);
        if (!entry) continue;

        bool is_selected = (scroll + i) == cursor;
        bool is_favorite = (entry->type == ENTRY_FILE) && favorites_is_favorite(entry->full_path);
        bool has_position = (entry->type == ENTRY_FILE) && (positions_get(entry->full_path) > 0);

        // Selection highlight (thick retro border)
        if (is_selected) {
            draw_retro_box(0, y - 6, g_screen_width, LINE_HEIGHT, 3, COLOR_HIGHLIGHT, COLOR_DIM);
        }

        // Icon prefix (ASCII-safe for embedded fonts)
        const char *prefix;
        if (entry->type == ENTRY_PARENT) {
            prefix = "[..] ";
        } else if (entry->type == ENTRY_DIRECTORY) {
            prefix = "[DIR] ";
        } else if (is_favorite) {
            prefix = "* ";   // Favorite
        } else {
            prefix = "> ";   // Regular file
        }

        // Build display text
        char display[300];
        snprintf(display, sizeof(display), "%s%s", prefix, entry->name);

        // Render with appropriate color
        // Priority: selected > played > favorite > normal
        SDL_Color color;
        if (is_selected) {
            color = has_position ? COLOR_PLAYED : COLOR_ACCENT;
        } else if (has_position) {
            color = COLOR_PLAYED;  // Orange for played files
        } else if (is_favorite) {
            color = COLOR_ACCENT;  // Green for favorites
        } else {
            color = COLOR_TEXT;    // White for unplayed
        }
        render_text_truncated(display, MARGIN, y, max_width, g_font_medium, color);

        y += LINE_HEIGHT;
    }

    // Footer with scroll indicator
    if (count > VISIBLE_ITEMS) {
        draw_rect(0, g_screen_height - FOOTER_HEIGHT, g_screen_width, 2, COLOR_DIM);

        char scroll_info[32];
        snprintf(scroll_info, sizeof(scroll_info), "%d/%d", cursor + 1, count);
        render_text(scroll_info, g_screen_width - 120 - 20, g_screen_height - 40, g_font_small, COLOR_DIM);
    }

    // Empty directory message
    if (count == 0) {
        render_text_centered("No music files found", g_screen_height / 2, g_font_medium, COLOR_DIM);
    }

    // Controls hint
    render_text("A:Select  Y:Fav  B:Up  Start:Menu", MARGIN, g_screen_height - 40, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

/**
 * Draw dark overlay for cover background (60% opacity)
 */
static void render_cover_overlay(void) {
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 153);  // 0.6 opacity = 153/255
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);
}

// Internal: renders player content without SDL_RenderPresent
static void ui_render_player_content(void) {
    const TrackInfo *info = audio_get_track_info();
    SDL_Texture *cover = cover_get_texture();
    bool has_cover_bg = (cover != NULL);

    // Adaptive colors for cover background (based on cover brightness)
    // Dark cover → light text, Light cover → dark text
    SDL_Color cover_text;
    SDL_Color cover_accent;
    bool dark_cover = cover_is_dark();
    if (has_cover_bg && !dark_cover) {
        // Light cover: use dark text for contrast
        cover_text = (SDL_Color){30, 30, 40, 255};      // Dark gray
        cover_accent = (SDL_Color){0, 100, 60, 255};    // Dark green
    } else {
        // Dark cover or no cover: use light text
        cover_text = (SDL_Color){255, 255, 255, 255};   // White
        cover_accent = (SDL_Color){51, 255, 51, 255};   // Neon green
    }

    // Background: either cover art or solid color
    if (has_cover_bg) {
        // Render cover as fullscreen background (cover fill - crop to fit)
        int tex_w, tex_h;
        SDL_QueryTexture(cover, NULL, NULL, &tex_w, &tex_h);

        // Calculate scaling to fill screen (cover mode, may crop)
        float scale_w = (float)g_screen_width / tex_w;
        float scale_h = (float)g_screen_height / tex_h;
        float scale = (scale_w > scale_h) ? scale_w : scale_h;

        int scaled_w = (int)(tex_w * scale);
        int scaled_h = (int)(tex_h * scale);

        // Center the scaled image (crop overflow)
        int offset_x = (g_screen_width - scaled_w) / 2;
        int offset_y = (g_screen_height - scaled_h) / 2;

        SDL_Rect dst = {offset_x, offset_y, scaled_w, scaled_h};
        SDL_RenderCopy(g_renderer, cover, NULL, &dst);

        // Apply overlay for readability (adjust based on cover brightness)
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        if (dark_cover) {
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 153);  // 60% dark overlay
        } else {
            SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 100);  // 40% light overlay
        }
        SDL_Rect overlay_rect = {0, 0, g_screen_width, g_screen_height};
        SDL_RenderFillRect(g_renderer, &overlay_rect);
    } else {
        // Solid background (no cover)
        SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(g_renderer);
    }

    // Header (with shadow when cover background)
    if (has_cover_bg) {
        render_text_shadow("> Mono", MARGIN, 8, g_font_medium, cover_text);
    } else {
        render_text("> Mono", MARGIN, 8, g_font_medium, COLOR_TEXT);
    }

    // Dancing monkey next to "Mono" (animates when playing)
    render_monkey(160, 6, audio_is_playing());

    // Favorite indicator if current track is a favorite
    const char *current_path = browser_get_selected_path();
    if (current_path && favorites_is_favorite(current_path)) {
        if (has_cover_bg) {
            render_text_shadow("*", 220, 8, g_font_medium, cover_accent);
        } else {
            render_text("*", 220, 8, g_font_medium, COLOR_ACCENT);
        }
    }

    // Status bar (volume + battery) in top right
    render_status_bar();

    // Header separator (only without cover background)
    if (!has_cover_bg) {
        draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);
    }

    // Now Playing section - centered layout when cover is background
    int center_y = has_cover_bg ? 220 : 150;

    // Title and Artist (with shadow when cover background)
    if (has_cover_bg) {
        // Centered layout with cover as background + text shadow
        render_text_centered_shadow(info->title, center_y, g_font_large, cover_text);
        render_text_centered_shadow(info->artist, center_y + 90, g_font_medium, cover_text);
    } else {
        // Original layout without cover
        render_text_centered(info->title, center_y, g_font_large, COLOR_TEXT);
        render_text_centered(info->artist, center_y + 90, g_font_medium, COLOR_DIM);
    }

    // Play/Pause indicator (use adaptive accent color)
    int play_icon_y = has_cover_bg ? center_y + 180 : center_y + 230;
    // Temporarily set accent color for play/pause icon
    SDL_Color icon_color = has_cover_bg ? cover_accent : COLOR_ACCENT;
    SDL_SetRenderDrawColor(g_renderer, icon_color.r, icon_color.g, icon_color.b, 255);
    render_play_pause_icon(g_screen_width / 2, play_icon_y, audio_is_playing());

    // Progress bar
    int bar_y = has_cover_bg ? g_screen_height - 130 : center_y + 390;
    int bar_x = MARGIN * 2;
    int bar_w = g_screen_width - MARGIN * 4;
    int bar_h = 12;

    // Background bar (semi-transparent when cover bg)
    if (has_cover_bg) {
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        if (dark_cover) {
            SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 40);
        } else {
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 40);
        }
        SDL_Rect bar_bg = {bar_x, bar_y, bar_w, bar_h};
        SDL_RenderFillRect(g_renderer, &bar_bg);
    } else {
        // Retro box for progress bar
        draw_retro_box(bar_x, bar_y, bar_w, bar_h, 2, COLOR_BG, COLOR_DIM);
    }

    // Progress fill
    if (info->duration_sec > 0) {
        float progress = (float)info->position_sec / info->duration_sec;
        int fill_w = (int)(bar_w * progress);

        // Fill inside the retro box (adjust for border if not cover mode)
        if (has_cover_bg) {
            draw_rect(bar_x, bar_y, fill_w, bar_h, cover_accent);
        } else {
            // Fill with 2px margin for border
            int margin = 2;
            if (fill_w > margin * 2) {
                draw_rect(bar_x + margin, bar_y + margin, fill_w - margin * 2, bar_h - margin * 2, COLOR_ACCENT);
            }
        }
    }

    // Time display
    char time_str[32];
    char pos_str[16], dur_str[16];
    format_time(info->position_sec, pos_str, sizeof(pos_str));

    if (info->duration_sec > 0) {
        format_time(info->duration_sec, dur_str, sizeof(dur_str));
    } else {
        snprintf(dur_str, sizeof(dur_str), "--:--");
    }
    snprintf(time_str, sizeof(time_str), "%s / %s", pos_str, dur_str);

    int tw, th;
    TTF_SizeUTF8(g_font_small, time_str, &tw, &th);
    SDL_Color time_color = has_cover_bg ? cover_text : COLOR_DIM;
    if (has_cover_bg) {
        render_text_shadow(time_str, (g_screen_width - tw) / 2, bar_y + 24, g_font_small, time_color);
    } else {
        render_text(time_str, (g_screen_width - tw) / 2, bar_y + 24, g_font_small, time_color);
    }

    // Footer with controls (only separator line without cover bg)
    if (!has_cover_bg) {
        draw_rect(0, g_screen_height - FOOTER_HEIGHT, g_screen_width, 2, COLOR_DIM);
    }

    // Control icons (with shadow when cover background)
    // Equal spacing: 150px between each item
    int ctrl_y = g_screen_height - 40;
    int ctrl_spacing = 150;
    int ctrl_x = MARGIN;
    SDL_Color ctrl_color = has_cover_bg ? cover_text : COLOR_DIM;
    if (has_cover_bg) {
        render_text_shadow("L:Prev", ctrl_x, ctrl_y, g_font_small, ctrl_color);
        render_text_shadow("A:Play", ctrl_x + ctrl_spacing, ctrl_y, g_font_small, ctrl_color);
        render_text_shadow("R:Next", ctrl_x + ctrl_spacing * 2, ctrl_y, g_font_small, ctrl_color);
        render_text_shadow("B:Back", ctrl_x + ctrl_spacing * 3, ctrl_y, g_font_small, ctrl_color);
#ifdef __APPLE__
        render_text_shadow("H:Help", ctrl_x + ctrl_spacing * 4, ctrl_y, g_font_small, ctrl_color);
#else
        render_text_shadow("X:Help", ctrl_x + ctrl_spacing * 4, ctrl_y, g_font_small, ctrl_color);
#endif
    } else {
        render_text("L:Prev", ctrl_x, ctrl_y, g_font_small, ctrl_color);
        render_text("A:Play", ctrl_x + ctrl_spacing, ctrl_y, g_font_small, ctrl_color);
        render_text("R:Next", ctrl_x + ctrl_spacing * 2, ctrl_y, g_font_small, ctrl_color);
        render_text("B:Back", ctrl_x + ctrl_spacing * 3, ctrl_y, g_font_small, ctrl_color);
#ifdef __APPLE__
        render_text("H:Help", ctrl_x + ctrl_spacing * 4, ctrl_y, g_font_small, ctrl_color);
#else
        render_text("X:Help", ctrl_x + ctrl_spacing * 4, ctrl_y, g_font_small, ctrl_color);
#endif
    }
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
    int menu_h = 420;  // Height for 6 items (Shuffle, Repeat, Sleep, Theme, YouTube, Exit)
    int menu_x = (g_screen_width - menu_w) / 2;
    int menu_y = (g_screen_height - menu_h) / 2;

    // Draw retro box for menu
    draw_retro_box(menu_x, menu_y, menu_w, menu_h, 4, COLOR_HIGHLIGHT, COLOR_TEXT);

    // Menu title
    render_text_centered("Options", menu_y + 20, g_font_medium, COLOR_TEXT);

    // Menu items (with spacing after title)
    int cursor = menu_get_cursor();
    int item_y = menu_y + 90;
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

    // Theme
    snprintf(buf, sizeof(buf), "Theme: %s", theme_get_current_name());
    if (cursor == MENU_THEME) {
        draw_rect(menu_x + 16, item_y - 4, menu_w - 32, item_h, COLOR_ACCENT);
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_BG);
    } else {
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_DIM);
    }
    item_y += item_h;

    // YouTube (show availability status)
    if (youtube_is_available()) {
        snprintf(buf, sizeof(buf), "YouTube Search");
    } else {
        snprintf(buf, sizeof(buf), "YouTube (unavailable)");
    }
    if (cursor == MENU_YOUTUBE) {
        draw_rect(menu_x + 16, item_y - 4, menu_w - 32, item_h, COLOR_ACCENT);
        render_text(buf, menu_x + 32, item_y, g_font_small, COLOR_BG);
    } else {
        render_text(buf, menu_x + 32, item_y, g_font_small,
                   youtube_is_available() ? COLOR_DIM : COLOR_ERROR);
    }
    item_y += item_h;

    // Close
    if (cursor == MENU_EXIT) {
        draw_rect(menu_x + 16, item_y - 4, menu_w - 32, item_h, COLOR_ACCENT);
        render_text("Close", menu_x + 32, item_y, g_font_small, COLOR_BG);
    } else {
        render_text("Close", menu_x + 32, item_y, g_font_small, COLOR_DIM);
    }

    // Controls hint
    render_text_centered("A:Select  B:Close", menu_y + menu_h - 40, g_font_small, COLOR_DIM);

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

    // Help box - calculate proper height with margins
    int box_w = 800;
    int title_space = 120;  // Space for title + gap
    int line_h = 42;        // Height per line
    int footer_space = 100; // Space for close hint
    int box_h = title_space + (line_count * line_h) + footer_space;
    int box_x = (g_screen_width - box_w) / 2;
    int box_y = (g_screen_height - box_h) / 2;

    draw_rect(box_x, box_y, box_w, box_h, COLOR_HIGHLIGHT);

    // Title (with proper spacing)
    render_text_centered(title, box_y + 30, g_font_medium, COLOR_ACCENT);

    // Control lines
    int line_y = box_y + title_space;
    for (int i = 0; i < line_count; i++) {
        render_text(lines[i], box_x + 40, line_y, g_font_small, COLOR_TEXT);
        line_y += line_h;
    }

    // Dismiss hint (X to close, like menu)
#ifdef __APPLE__
    render_text_centered("H:Close", box_y + box_h - 60, g_font_small, COLOR_DIM);
#else
    render_text_centered("X:Close", box_y + box_h - 60, g_font_small, COLOR_DIM);
#endif

    SDL_RenderPresent(g_renderer);
}

void ui_render_help_browser(void) {
    // First render browser underneath
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);
    // Simplified background, then overlay

    const char *lines[] = {
        "D-Pad      Navigate list",
        "A          Open / Play",
        "B          Go up folder",
        "Y          Toggle favorite",
        "Select     File menu (Rename/Delete)",
        "Start      Options menu",
        "Start+B    Exit app",
        "---        Legend ---",
        "[..]       Parent folder",
        "*          Favorite",
        "Orange     Played before"
    };

    render_help_overlay("File Browser", lines, 11);
}

void ui_render_help_player(void) {
    // Render player content underneath
    ui_render_player_content();

    const char *lines[] = {
        "A          Play / Pause",
        "B          Back to browser",
        "L / R      Prev / Next track",
        "D-Pad L/R  Seek (hold=faster)",
        "D-Pad U/D  Volume",
        "Y          Toggle favorite",
        "Select     Dim screen",
        "Start      Options menu",
        "Start+B    Exit app"
    };

    render_help_overlay("Now Playing", lines, 9);
}

void ui_render_loading(const char *filename) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_text("> Mono", MARGIN, 8, g_font_medium, COLOR_TEXT);

    // Loading message (centered)
    render_text_centered("Loading...", g_screen_height / 2 - 40, g_font_large, COLOR_ACCENT);

    // Filename (centered, below loading)
    if (filename && filename[0]) {
        render_text_centered(filename, g_screen_height / 2 + 40, g_font_medium, COLOR_DIM);
    }

    SDL_RenderPresent(g_renderer);
}

void ui_render_scanning(int current, int total, const char *current_file, int found) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_text("> Mono", MARGIN, 8, g_font_medium, COLOR_TEXT);

    // Title
    render_text_centered("Scanning Metadata...", g_screen_height / 2 - 100, g_font_large, COLOR_ACCENT);

    // Progress bar
    int bar_w = 500;
    int bar_h = 30;
    int bar_x = (g_screen_width - bar_w) / 2;
    int bar_y = g_screen_height / 2 - 30;

    // Background
    draw_rect(bar_x, bar_y, bar_w, bar_h, COLOR_DIM);

    // Progress fill
    if (total > 0) {
        int fill_w = (bar_w - 4) * current / total;
        draw_rect(bar_x + 2, bar_y + 2, fill_w, bar_h - 4, COLOR_ACCENT);
    }

    // Progress text
    char progress[64];
    snprintf(progress, sizeof(progress), "%d / %d", current, total);
    render_text_centered(progress, bar_y + bar_h + 20, g_font_medium, COLOR_TEXT);

    // Current file
    if (current_file && current_file[0]) {
        char file_text[128];
        snprintf(file_text, sizeof(file_text), "%.50s%s",
                 current_file, strlen(current_file) > 50 ? "..." : "");
        render_text_centered(file_text, bar_y + bar_h + 60, g_font_small, COLOR_DIM);
    }

    // Found count
    char found_text[64];
    snprintf(found_text, sizeof(found_text), "Found: %d", found);
    render_text_centered(found_text, bar_y + bar_h + 100, g_font_medium, COLOR_HIGHLIGHT);

    // Cancel hint
    render_text_centered("B: Cancel", g_screen_height - 60, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

void ui_render_scan_complete(int found, int total) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_text("> Mono", MARGIN, 8, g_font_medium, COLOR_TEXT);

    // Title
    render_text_centered("Scan Complete!", g_screen_height / 2 - 60, g_font_large, COLOR_ACCENT);

    // Results
    char results[128];
    snprintf(results, sizeof(results), "Found metadata for %d of %d files", found, total);
    render_text_centered(results, g_screen_height / 2 + 20, g_font_medium, COLOR_TEXT);

    // Hint
    render_text_centered("Press any button to continue", g_screen_height - 100, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

void ui_render_error(const char *message) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_text("> Mono", MARGIN, 8, g_font_medium, COLOR_TEXT);

    // Error icon/text (centered)
    render_text_centered("Error", g_screen_height / 2 - 60, g_font_large, COLOR_ERROR);

    // Error message (centered, below error)
    if (message && message[0]) {
        render_text_centered(message, g_screen_height / 2 + 20, g_font_medium, COLOR_DIM);
    }

    // Hint
    render_text_centered("Press any button to continue", g_screen_height - 100, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

void ui_render_file_menu(void) {
    // Render browser underneath
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Darken overlay (same as help overlay)
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 200);
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);

    // Calculate dynamic box size (same style as help overlay)
    bool is_dir = filemenu_is_directory();
    bool has_backup = metadata_has_backup();

    // Option count: Files=3 (Rename,Delete,Cancel), Dirs=4 or 5 (with Restore if backup)
    int option_count = is_dir ? (has_backup ? 5 : 4) : 3;

    int box_w = 600;
    int title_space = 120;  // Space for title + gap
    int line_h = 42;        // Height per line (same as help overlay)
    int footer_space = 100; // Space for close hint (same as help overlay)
    int box_h = title_space + (option_count * line_h) + footer_space;
    int box_x = (g_screen_width - box_w) / 2;
    int box_y = (g_screen_height - box_h) / 2;

    draw_rect(box_x, box_y, box_w, box_h, COLOR_HIGHLIGHT);

    // Title with filename (same style as help overlay title)
    const char *filename = filemenu_get_filename();
    char title[128];
    snprintf(title, sizeof(title), "%.40s%s", filename, strlen(filename) > 40 ? "..." : "");
    render_text_centered(title, box_y + 30, g_font_medium, COLOR_ACCENT);

    // Type indicator
    const char *type_str = is_dir ? "[Folder]" : "[File]";
    render_text_centered(type_str, box_y + 70, g_font_small, COLOR_DIM);

    // Menu options (same line spacing as help overlay)
    int cursor = filemenu_get_cursor();
    int line_y = box_y + title_space;

    // Build options list based on type and backup status
    const char *dir_options_backup[] = {"Rename", "Delete", "Scan Metadata", "Restore Metadata", "Cancel"};
    const char *dir_options_no_backup[] = {"Rename", "Delete", "Scan Metadata", "Cancel"};
    const char *file_options[] = {"Rename", "Delete", "Cancel"};

    const char **options;
    if (!is_dir) {
        options = file_options;
    } else if (has_backup) {
        options = dir_options_backup;
    } else {
        options = dir_options_no_backup;
    }

    for (int i = 0; i < option_count; i++) {
        // Render option (same style as help overlay lines)
        SDL_Color color = (i == cursor) ? COLOR_ACCENT : COLOR_TEXT;
        char option_text[64];
        snprintf(option_text, sizeof(option_text), "%s  %s",
                 (i == cursor) ? ">" : " ", options[i]);
        render_text(option_text, box_x + 40, line_y, g_font_small, color);
        line_y += line_h;
    }

    // Controls hint (same position as help overlay)
    render_text_centered("A:Select  B:Cancel", box_y + box_h - 60, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

void ui_render_confirm_delete(void) {
    // Darken overlay
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 220);
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);

    // Calculate dynamic box size (same style as help overlay)
    bool is_dir = filemenu_is_directory();
    int content_lines = is_dir ? 2 : 1;  // Filename + optional warning

    int box_w = 700;
    int title_space = 100;   // Space for "Delete?" title
    int line_h = 50;         // Height per content line
    int footer_space = 80;   // Space for controls hint
    int box_h = title_space + (content_lines * line_h) + footer_space;
    int box_x = (g_screen_width - box_w) / 2;
    int box_y = (g_screen_height - box_h) / 2;

    draw_rect(box_x, box_y, box_w, box_h, COLOR_HIGHLIGHT);

    // Warning title (with proper spacing)
    render_text_centered("Delete?", box_y + 30, g_font_large, COLOR_ERROR);

    // Filename
    const char *filename = filemenu_get_filename();
    char display[64];
    snprintf(display, sizeof(display), "%.50s%s", filename, strlen(filename) > 50 ? "..." : "");
    int content_y = box_y + title_space;
    render_text_centered(display, content_y, g_font_medium, COLOR_TEXT);

    // Warning for directory
    if (is_dir) {
        render_text_centered("(All contents will be deleted)", content_y + line_h, g_font_small, COLOR_DIM);
    }

    // Controls hint
    render_text_centered("A:Confirm  B:Cancel", box_y + box_h - 50, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

void ui_render_rename(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Title
    render_text_centered("Rename", 20, g_font_medium, COLOR_ACCENT);

    // Current text with cursor
    const char *text = filemenu_rename_get_text();
    int cursor_pos = filemenu_rename_get_cursor();
    int text_y = 80;

    // Text box background
    int box_w = g_screen_width - 80;
    int box_x = 40;
    draw_rect(box_x, text_y - 8, box_w, 50, COLOR_HIGHLIGHT);

    // Render text with cursor indicator
    char display[300];
    snprintf(display, sizeof(display), "%.*s|%s", cursor_pos, text, text + cursor_pos);
    render_text(display, box_x + 12, text_y, g_font_small, COLOR_TEXT);

    // Grid keyboard (QWERTY layout)
    int kbd_cols, kbd_rows;
    filemenu_rename_get_kbd_size(&kbd_cols, &kbd_rows);
    int cur_row, cur_col;
    filemenu_rename_get_kbd_pos(&cur_row, &cur_col);

    int kbd_y = 150;
    int cell_w = 100;
    int cell_h = 70;
    int kbd_w = kbd_cols * cell_w;
    int kbd_x = (g_screen_width - kbd_w) / 2;

    for (int row = 0; row < kbd_rows; row++) {
        for (int col = 0; col < kbd_cols; col++) {
            char c = filemenu_rename_get_char_at(row, col);
            if (c == '\0') continue;

            int x = kbd_x + col * cell_w;
            int y = kbd_y + row * cell_h;

            bool is_selected = (row == cur_row && col == cur_col);

            // Draw cell background
            if (is_selected) {
                draw_rect(x + 2, y + 2, cell_w - 4, cell_h - 4, COLOR_ACCENT);
            }

            // Draw character
            char ch_str[2] = {c, '\0'};
            if (c == ' ') {
                // Show "SPC" for space
                strcpy(ch_str, "_");
            }

            int tw, th;
            TTF_SizeUTF8(g_font_small, ch_str, &tw, &th);
            int tx = x + (cell_w - tw) / 2;
            int ty = y + (cell_h - th) / 2;

            SDL_Color color = is_selected ? COLOR_BG : COLOR_TEXT;
            render_text(ch_str, tx, ty, g_font_small, color);
        }
    }

    // Controls
    int ctrl_y = g_screen_height - 80;
    render_text_centered("D-Pad: Select   A: Insert   Y: Delete", ctrl_y, g_font_small, COLOR_DIM);
    render_text_centered("Start: Confirm   B: Cancel", ctrl_y + 35, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

// ============================================================================
// YouTube UI Rendering
// ============================================================================

void ui_render_youtube_search(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Check if we're in SEARCHING state - show loading indicator
    if (ytsearch_get_state() == YTSEARCH_SEARCHING) {
        // Title
        render_text_centered("YouTube Search", 20, g_font_medium, COLOR_ACCENT);

        // Show searching message with animated monkey
        render_text_centered("Searching...", g_screen_height / 2 - 80, g_font_large, COLOR_ACCENT);

        // Show query being searched
        const char *query = ytsearch_get_query();
        if (query && query[0]) {
            char display[128];
            snprintf(display, sizeof(display), "\"%s\"", query);
            render_text_centered(display, g_screen_height / 2 - 20, g_font_medium, COLOR_TEXT);
        }

        // Animated monkey (always animating during search)
        int monkey_x = (g_screen_width - 16 * MONKEY_PIXEL_SIZE) / 2;
        int monkey_y = g_screen_height / 2 + 40;
        render_monkey(monkey_x, monkey_y, true);  // true = animate

        // Hint
        render_text_centered("Please wait...", g_screen_height - 100, g_font_small, COLOR_DIM);

        SDL_RenderPresent(g_renderer);
        return;
    }

    // Title
    render_text_centered("YouTube Search", 20, g_font_medium, COLOR_ACCENT);

    // Current query with cursor
    const char *query = ytsearch_get_query();
    int cursor_pos = ytsearch_get_cursor();
    int text_y = 80;

    // Text box background
    int box_w = g_screen_width - 80;
    int box_x = 40;
    draw_rect(box_x, text_y - 8, box_w, 50, COLOR_HIGHLIGHT);

    // Render query with cursor indicator
    char display[300];
    if (query[0]) {
        snprintf(display, sizeof(display), "%.*s|%s", cursor_pos, query, query + cursor_pos);
    } else {
        snprintf(display, sizeof(display), "|");
    }
    render_text(display, box_x + 12, text_y, g_font_small, COLOR_TEXT);

    // Grid keyboard (QWERTY layout)
    int kbd_cols, kbd_rows;
    ytsearch_get_kbd_size(&kbd_cols, &kbd_rows);
    int cur_row, cur_col;
    ytsearch_get_kbd_pos(&cur_row, &cur_col);

    int kbd_y = 150;
    int cell_w = 100;
    int cell_h = 70;
    int kbd_w = kbd_cols * cell_w;
    int kbd_x = (g_screen_width - kbd_w) / 2;

    for (int row = 0; row < kbd_rows; row++) {
        for (int col = 0; col < kbd_cols; col++) {
            char c = ytsearch_get_char_at(row, col);
            if (c == '\0') continue;

            int x = kbd_x + col * cell_w;
            int y = kbd_y + row * cell_h;

            bool is_selected = (row == cur_row && col == cur_col);

            // Draw cell background
            if (is_selected) {
                draw_rect(x + 2, y + 2, cell_w - 4, cell_h - 4, COLOR_ACCENT);
            }

            // Draw character
            char ch_str[2] = {c, '\0'};
            if (c == ' ') {
                // Show "_" for space
                strcpy(ch_str, "_");
            }

            int tw, th;
            TTF_SizeUTF8(g_font_small, ch_str, &tw, &th);
            int tx = x + (cell_w - tw) / 2;
            int ty = y + (cell_h - th) / 2;

            SDL_Color color = is_selected ? COLOR_BG : COLOR_TEXT;
            render_text(ch_str, tx, ty, g_font_small, color);
        }
    }

    // Error message if any
    const char *error = ytsearch_get_error();
    if (error) {
        render_text_centered(error, kbd_y + kbd_rows * cell_h + 10, g_font_small, COLOR_ERROR);
    }

    // Controls
    int ctrl_y = g_screen_height - 100;
    render_text_centered("D-Pad: Select   A: Insert   Y: Delete", ctrl_y, g_font_small, COLOR_DIM);
    render_text_centered("Start: Search   B: Cancel", ctrl_y + 30, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

// Scroll state for YouTube results title
static int g_yt_scroll_offset = 0;
static int g_yt_scroll_cursor = -1;
static Uint32 g_yt_scroll_last_update = 0;
#define YT_SCROLL_SPEED_MS 100  // ms per character scroll
#define YT_SCROLL_PAUSE_MS 1500 // pause at start/end

void ui_render_youtube_results(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    const char *query = ytsearch_get_query();
    char header[128];
    snprintf(header, sizeof(header), "Results: %s", query);
    render_text(header, MARGIN, 8, g_font_small, COLOR_TEXT);

    // Results list
    int result_count = ytsearch_get_result_count();
    int cursor = ytsearch_get_results_cursor();
    int scroll = ytsearch_get_scroll_offset();

    // Reset scroll when cursor changes
    if (cursor != g_yt_scroll_cursor) {
        g_yt_scroll_cursor = cursor;
        g_yt_scroll_offset = 0;
        g_yt_scroll_last_update = SDL_GetTicks() + YT_SCROLL_PAUSE_MS;
    }

    int y = HEADER_HEIGHT + 10;
    int visible = 8;  // More visible results with smaller font
    int max_title_chars = 55;  // Max chars that fit on screen with small font

    for (int i = 0; i < visible && (scroll + i) < result_count; i++) {
        int idx = scroll + i;
        const YouTubeResult *result = ytsearch_get_result(idx);
        if (!result) continue;

        bool is_selected = (idx == cursor);

        // Highlight selected item
        if (is_selected) {
            draw_rect(0, y - 5, g_screen_width, LINE_HEIGHT + 10, COLOR_HIGHLIGHT);
        }

        // Format duration
        char duration_str[16];
        youtube_format_duration(result->duration_sec, duration_str);

        // Title with scroll for selected long titles
        char title_display[256];
        int title_len = strlen(result->title);

        if (is_selected && title_len > max_title_chars) {
            // Animate scroll for selected item with long title
            Uint32 now = SDL_GetTicks();
            if (now > g_yt_scroll_last_update) {
                if (now - g_yt_scroll_last_update > YT_SCROLL_SPEED_MS) {
                    g_yt_scroll_offset++;
                    g_yt_scroll_last_update = now;
                    // Reset at end of title
                    if (g_yt_scroll_offset > title_len - max_title_chars + 10) {
                        g_yt_scroll_offset = 0;
                        g_yt_scroll_last_update = now + YT_SCROLL_PAUSE_MS;
                    }
                }
            }

            // Show scrolled portion
            int start = g_yt_scroll_offset;
            if (start > title_len) start = 0;
            snprintf(title_display, sizeof(title_display), "%.*s",
                     max_title_chars, result->title + start);
        } else if (title_len > max_title_chars) {
            // Truncate non-selected long titles
            snprintf(title_display, sizeof(title_display), "%.*s...",
                     max_title_chars - 3, result->title);
        } else {
            strncpy(title_display, result->title, sizeof(title_display) - 1);
            title_display[sizeof(title_display) - 1] = '\0';
        }

        // Render title (smaller font)
        render_text(title_display, MARGIN + 10, y,
                   g_font_small, is_selected ? COLOR_ACCENT : COLOR_TEXT);

        // Render channel and duration on next line
        char meta_display[128];
        snprintf(meta_display, sizeof(meta_display), "%.35s  [%s]",
                 result->channel, duration_str);
        render_text(meta_display, MARGIN + 20, y + 28,
                   g_font_small, COLOR_DIM);

        y += LINE_HEIGHT + 10;  // Reduced spacing with smaller font
    }

    // Scroll indicators
    if (scroll > 0) {
        render_text_centered("^ more ^", HEADER_HEIGHT - 5, g_font_small, COLOR_DIM);
    }
    if (scroll + visible < result_count) {
        render_text_centered("v more v", g_screen_height - 70, g_font_small, COLOR_DIM);
    }

    // Footer
    char footer[64];
    snprintf(footer, sizeof(footer), "%d of %d", cursor + 1, result_count);
    render_text_centered(footer, g_screen_height - 45, g_font_small, COLOR_DIM);

    // Controls hint
    render_text("A: Download   B: Back", MARGIN, g_screen_height - 25, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}

void ui_render_youtube_download(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_text("> YouTube", MARGIN, 8, g_font_medium, COLOR_TEXT);

    // Title
    render_text_centered("Downloading...", g_screen_height / 2 - 120, g_font_large, COLOR_ACCENT);

    // Track title
    const char *title = ytsearch_get_download_title();
    if (title) {
        char title_display[64];
        if (strlen(title) > 40) {
            snprintf(title_display, sizeof(title_display), "%.37s...", title);
        } else {
            strncpy(title_display, title, sizeof(title_display) - 1);
            title_display[sizeof(title_display) - 1] = '\0';
        }
        render_text_centered(title_display, g_screen_height / 2 - 60, g_font_medium, COLOR_TEXT);
    }

    // Progress bar
    int bar_w = 500;
    int bar_h = 30;
    int bar_x = (g_screen_width - bar_w) / 2;
    int bar_y = g_screen_height / 2;

    // Background
    draw_rect(bar_x, bar_y, bar_w, bar_h, COLOR_DIM);

    // Progress fill
    int progress = ytsearch_get_download_progress();
    if (progress > 0) {
        int fill_w = (bar_w - 4) * progress / 100;
        draw_rect(bar_x + 2, bar_y + 2, fill_w, bar_h - 4, COLOR_ACCENT);
    }

    // Progress text
    char progress_text[32];
    snprintf(progress_text, sizeof(progress_text), "%d%%", progress);
    render_text_centered(progress_text, bar_y + bar_h + 20, g_font_medium, COLOR_TEXT);

    // Status
    const char *status = ytsearch_get_download_status();
    if (status) {
        render_text_centered(status, bar_y + bar_h + 70, g_font_small, COLOR_DIM);
    }

    // Cancel hint
    render_text_centered("B: Cancel", g_screen_height - 60, g_font_small, COLOR_DIM);

    SDL_RenderPresent(g_renderer);
}
