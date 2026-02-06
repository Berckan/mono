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
#include "download_queue.h"
#include "equalizer.h"
#include "spotify.h"
#include "spsearch.h"
#include "spotify_audio.h"
#include "update.h"
#include "version.h"
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
static TTF_Font *g_font_tiny = NULL;    // 16px for watermarks
static TTF_Font *g_font_hint = NULL;    // 22px for footer hints

// Theme color accessors (macros for cleaner code)
#define COLOR_BG (theme_get_colors()->bg)
#define COLOR_TEXT (theme_get_colors()->text)
#define COLOR_DIM (theme_get_colors()->dim)
#define COLOR_ACCENT (theme_get_colors()->accent)
#define COLOR_HIGHLIGHT (theme_get_colors()->highlight)
#define COLOR_ERROR (theme_get_colors()->error)

// Layout constants (compact for 720p)
#define SCREEN_PAD 10      // Uniform padding from all screen edges
#define HEADER_HEIGHT 66
#define FOOTER_HEIGHT 52
#define MARGIN SCREEN_PAD
#define LINE_HEIGHT 60

// Number of visible items in browser list
#define VISIBLE_ITEMS 9

// Button labels (keyboard letters on macOS, gamepad on device)
#ifdef __APPLE__
#define BTN_A "Z"
#define BTN_B "X"
#define BTN_X "H"
#define BTN_Y "F"
#define BTN_START "Enter"
#define BTN_SELECT "Shift"
#else
#define BTN_A "A"
#define BTN_B "B"
#define BTN_X "X"
#define BTN_Y "Y"
#define BTN_START "Start"
#define BTN_SELECT "Select"
#endif

// Dancing monkey animation (16x16 pixels, 3 frames)
// 0 = transparent, 1 = brown (fur), 2 = beige (face/belly), 3 = black (eyes), 4 = yellow (banana)
// Monkey with banana, classic pixel art style
static const uint8_t MONKEY_FRAMES[3][16][16] = {
    // Frame 0: Right arm raised, waving
    {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,0,1,1,2,3,2,2,3,2,1,1,2,0,0},
        {0,0,0,1,1,2,2,2,2,2,2,1,1,1,0,0},
        {0,0,0,0,1,2,2,2,2,2,2,1,1,0,0,0},
        {0,0,0,0,1,1,2,2,2,2,1,1,0,0,0,0},
        {0,0,0,1,1,1,2,2,2,2,1,1,1,0,0,0},
        {0,0,0,0,1,1,2,2,2,2,1,1,0,0,0,0},
        {0,0,0,1,1,1,2,4,2,2,1,1,0,0,0,0},
        {0,0,0,1,1,1,4,1,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,4,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    },
    // Frame 1: Arms down, resting pose
    {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,0,1,1,2,3,2,2,3,2,1,1,0,0,0},
        {0,0,0,1,1,2,2,2,2,2,2,1,1,0,0,0},
        {0,0,0,0,1,2,2,2,2,2,2,1,0,0,0,0},
        {0,0,0,0,1,1,2,2,2,2,1,1,0,0,0,0},
        {0,0,0,1,1,1,2,2,2,2,1,1,1,0,0,0},
        {0,0,0,0,1,1,2,2,2,2,1,1,0,2,0,0},
        {0,0,0,1,1,1,2,4,2,2,1,1,1,1,0,0},
        {0,0,0,1,1,1,4,1,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,4,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    },
    // Frame 2: Both arms extended
    {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,0,1,1,2,3,2,2,3,2,1,1,0,0,0},
        {0,0,0,1,1,2,2,2,2,2,2,1,1,0,0,0},
        {0,0,0,0,1,2,2,2,2,2,2,1,0,0,0,0},
        {0,0,0,0,1,1,2,2,2,2,1,1,0,0,0,0},
        {0,0,1,1,1,1,2,2,2,2,1,1,1,1,0,0},
        {0,2,0,0,1,1,2,2,2,2,1,1,0,0,2,0},
        {0,0,0,0,1,1,2,2,2,2,1,1,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    },
};

// Animation state
static int g_monkey_frame = 0;
static int g_monkey_seq = 0;
static Uint32 g_monkey_last_update = 0;
#define MONKEY_FRAME_MS 200  // ms per frame
#define MONKEY_PIXEL_SIZE 3  // scale factor

// Dance sequence: arm up → arms down → arm up → T-pose → repeat
static const int MONKEY_DANCE_SEQ[] = {0, 1, 0, 2};
#define MONKEY_DANCE_LEN 4

// Toast notification state
static char g_toast_message[128] = {0};
static Uint32 g_toast_start_time = 0;
#define UI_TOAST_DURATION_MS 2000

// Player text scroll state (for long titles/artists)
static int g_player_title_scroll = 0;
static int g_player_artist_scroll = 0;
static Uint32 g_player_scroll_last_update = 0;
static char g_player_last_title[256] = {0};  // Detect song change

#define PLAYER_SCROLL_SPEED_MS 80       // Faster than YT results
#define PLAYER_SCROLL_PAUSE_MS 2000     // Pause at start/end
#define PLAYER_SCROLL_GAP "   •   "     // Visual separator between repetitions

// Forward declarations for overlays
static void render_toast_overlay(void);
static void render_version_watermark(void);

// Monkey color palette (indexed by pixel values in MONKEY_FRAMES)
// 0 = transparent (not rendered)
static const SDL_Color MONKEY_PALETTE[] = {
    {  0,   0,   0,   0},  // 0: transparent (unused, skip in render)
    {139,  69,  19, 255},  // 1: brown - fur (#8B4513)
    {210, 180, 140, 255},  // 2: beige - face/belly (#D2B48C)
    {  0,   0,   0, 255},  // 3: black - eyes (#000000)
    {255, 215,   0, 255},  // 4: yellow - banana (#FFD700)
};

// ============================================================================
// TEXT CACHING SYSTEM
// Reduces TTF_RenderUTF8_Blended calls from 50+/frame to ~5/frame
// LRU cache with 64 entries, expires textures after 5 seconds of non-use
// ============================================================================
#define TEXT_CACHE_SIZE 64
#define TEXT_CACHE_EXPIRE_MS 5000  // 5 seconds

typedef struct {
    char text[128];
    SDL_Color color;
    TTF_Font *font;
    SDL_Texture *texture;
    int width, height;
    Uint32 last_used;
    bool in_use;
} CachedText;

static CachedText g_text_cache[TEXT_CACHE_SIZE];
static int g_text_cache_hits = 0;
static int g_text_cache_misses = 0;

/**
 * Initialize text cache
 */
static void text_cache_init(void) {
    memset(g_text_cache, 0, sizeof(g_text_cache));
}

/**
 * Clear all cached textures (call on theme change or cleanup)
 */
static void text_cache_clear(void) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (g_text_cache[i].texture) {
            SDL_DestroyTexture(g_text_cache[i].texture);
            g_text_cache[i].texture = NULL;
        }
        g_text_cache[i].in_use = false;
    }
}

/**
 * Expire old cache entries (call periodically)
 */
static void text_cache_expire(void) {
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (g_text_cache[i].in_use &&
            now - g_text_cache[i].last_used > TEXT_CACHE_EXPIRE_MS) {
            if (g_text_cache[i].texture) {
                SDL_DestroyTexture(g_text_cache[i].texture);
                g_text_cache[i].texture = NULL;
            }
            g_text_cache[i].in_use = false;
        }
    }
}

/**
 * Find or create cached texture for text
 * @return Texture pointer (do NOT destroy - managed by cache)
 */
static SDL_Texture* text_cache_get(const char *text, TTF_Font *font, SDL_Color color,
                                   int *out_w, int *out_h) {
    if (!text || !text[0]) return NULL;

    Uint32 now = SDL_GetTicks();

    // Search for existing entry
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (g_text_cache[i].in_use &&
            g_text_cache[i].font == font &&
            g_text_cache[i].color.r == color.r &&
            g_text_cache[i].color.g == color.g &&
            g_text_cache[i].color.b == color.b &&
            strcmp(g_text_cache[i].text, text) == 0) {
            // Cache hit
            g_text_cache[i].last_used = now;
            g_text_cache_hits++;
            *out_w = g_text_cache[i].width;
            *out_h = g_text_cache[i].height;
            return g_text_cache[i].texture;
        }
    }

    // Cache miss - find free slot or evict oldest
    int slot = -1;
    Uint32 oldest_time = now;

    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (!g_text_cache[i].in_use) {
            slot = i;
            break;
        }
        if (g_text_cache[i].last_used < oldest_time) {
            oldest_time = g_text_cache[i].last_used;
            slot = i;
        }
    }

    if (slot < 0) slot = 0;  // Shouldn't happen, but safety first

    // Evict old entry if needed
    if (g_text_cache[slot].texture) {
        SDL_DestroyTexture(g_text_cache[slot].texture);
    }

    // Create new entry
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return NULL;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return NULL;
    }

    // Store in cache
    strncpy(g_text_cache[slot].text, text, sizeof(g_text_cache[slot].text) - 1);
    g_text_cache[slot].text[sizeof(g_text_cache[slot].text) - 1] = '\0';
    g_text_cache[slot].color = color;
    g_text_cache[slot].font = font;
    g_text_cache[slot].texture = texture;
    g_text_cache[slot].width = surface->w;
    g_text_cache[slot].height = surface->h;
    g_text_cache[slot].last_used = now;
    g_text_cache[slot].in_use = true;

    SDL_FreeSurface(surface);
    g_text_cache_misses++;

    *out_w = g_text_cache[slot].width;
    *out_h = g_text_cache[slot].height;
    return texture;
}

/**
 * Render dancing monkey sprite
 */
static void render_monkey(int x, int y, bool is_playing) {
    Uint32 now = SDL_GetTicks();

    // Update animation frame if playing
    if (is_playing) {
        if (now - g_monkey_last_update > MONKEY_FRAME_MS) {
            g_monkey_seq = (g_monkey_seq + 1) % MONKEY_DANCE_LEN;
            g_monkey_frame = MONKEY_DANCE_SEQ[g_monkey_seq];
            g_monkey_last_update = now;
        }
    } else {
        g_monkey_frame = 1;  // Resting pose (arms down) when not playing
        g_monkey_seq = 0;
    }

    const uint8_t (*frame)[16] = MONKEY_FRAMES[g_monkey_frame];

    for (int py = 0; py < 16; py++) {
        for (int px = 0; px < 16; px++) {
            uint8_t pixel = frame[py][px];
            if (pixel == 0) continue;  // Transparent

            SDL_Color color = MONKEY_PALETTE[pixel];

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

// Forward declaration for render_text_shadow (calls render_text)
static void render_text_shadow(const char *text, int x, int y, TTF_Font *font, SDL_Color color);

/**
 * Render text to screen (cached version)
 * Uses LRU text cache to avoid repeated TTF rendering
 */
static void render_text(const char *text, int x, int y, TTF_Font *font, SDL_Color color) {
    if (!text || !text[0]) return;

    int w, h;
    SDL_Texture *texture = text_cache_get(text, font, color, &w, &h);
    if (texture) {
        SDL_Rect dest = {x, y, w, h};
        SDL_RenderCopy(g_renderer, texture, NULL, &dest);
    }
}

/**
 * Render text with shadow (for cover background readability)
 */
static void render_text_shadow(const char *text, int x, int y, TTF_Font *font, SDL_Color color) {
    if (!text || !text[0]) return;

    SDL_Color shadow = {0, 0, 0, 180};

    SDL_Surface *shadow_surface = TTF_RenderUTF8_Blended(font, text, shadow);
    if (shadow_surface) {
        SDL_Texture *shadow_tex = SDL_CreateTextureFromSurface(g_renderer, shadow_surface);
        if (shadow_tex) {
            SDL_SetTextureBlendMode(shadow_tex, SDL_BLENDMODE_BLEND);
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

    render_text(text, x, y, font, color);
}

/**
 * Render screen header: title text + monkey at end of text
 */
static void render_header(const char *title, SDL_Color color, bool animate_monkey) {
    render_text(title, SCREEN_PAD, SCREEN_PAD, g_font_medium, color);
    int text_w;
    TTF_SizeUTF8(g_font_medium, title, &text_w, NULL);
    render_monkey(SCREEN_PAD + text_w + 8, SCREEN_PAD - 2, animate_monkey);
}

/**
 * Render screen header with shadow: title text + monkey at end of text
 */
static void render_header_shadow(const char *title, SDL_Color color, bool animate_monkey) {
    render_text_shadow(title, SCREEN_PAD, SCREEN_PAD, g_font_medium, color);
    int text_w;
    TTF_SizeUTF8(g_font_medium, title, &text_w, NULL);
    render_monkey(SCREEN_PAD + text_w + 8, SCREEN_PAD - 2, animate_monkey);
}

/**
 * Get the width of the header (title + monkey) for positioning elements after it
 */
static int get_header_end_x(const char *title) {
    int text_w;
    TTF_SizeUTF8(g_font_medium, title, &text_w, NULL);
    return SCREEN_PAD + text_w + 8 + (16 * MONKEY_PIXEL_SIZE) + 8;
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
    g_font_tiny = TTF_OpenFont(found_path, 16);
    g_font_hint = TTF_OpenFont(found_path, 22);

    if (!g_font_large || !g_font_medium || !g_font_small || !g_font_tiny || !g_font_hint) {
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
    int text_y = SCREEN_PAD;  // Vertical position for text
    int batt_y = SCREEN_PAD + 15;  // Battery icon vertically centered with text
    int right_margin = SCREEN_PAD;  // Keep away from screen edge

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

    // Initialize text cache
    text_cache_init();

    return 0;
}

void ui_cleanup(void) {
    text_cache_clear();  // Clear text cache before destroying renderer
    cover_cleanup();
    if (g_font_large) TTF_CloseFont(g_font_large);
    if (g_font_medium) TTF_CloseFont(g_font_medium);
    if (g_font_small) TTF_CloseFont(g_font_small);
    if (g_font_tiny) TTF_CloseFont(g_font_tiny);
    if (g_font_hint) TTF_CloseFont(g_font_hint);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
}

// External getters from main.c for home/resume/favorites state
extern int home_get_cursor(void);
extern int resume_get_cursor(void);
extern int resume_get_scroll(void);
extern int favorites_get_cursor(void);
extern int favorites_get_scroll(void);

// Number of visible items in resume/favorites lists
#define HOME_LIST_VISIBLE 8

void ui_render_home(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Home", COLOR_TEXT, false);

    // Status bar (volume + battery) in top right
    render_status_bar();

    // Draw header separator
    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // Get counts for display
    int resume_count = positions_get_count();
    int favorites_count = favorites_get_count();
    int cursor = home_get_cursor();

    // Menu items layout - responsive to screen height
    // Available space: between header (60) and footer (50)
    int content_height = g_screen_height - HEADER_HEIGHT - FOOTER_HEIGHT;
    int menu_count = 5;
    int menu_start_y = HEADER_HEIGHT + content_height / 10;
    int item_height = content_height / 7;  // Each item takes ~1/7 of space (5 items)
    int box_width = g_screen_width / 3;  // 1/3 of screen width
    int box_height = item_height - 12;  // Leave some padding
    int box_x = (g_screen_width - box_width) / 2;

    // Menu items (5 options)
    const char *labels[] = {"Resume", "Browse", "Favorites", "YouTube", "Spotify (Soon)"};
    int counts[] = {resume_count, -1, favorites_count, -1, -1};  // -1 means no count shown
    bool yt_available = youtube_is_available();
    bool sp_available = false;  // Disabled until Spotify Developer API reopens

    for (int i = 0; i < menu_count; i++) {
        int y = menu_start_y + i * item_height;
        bool is_selected = (i == cursor);
        // Resume/Favorites disabled if empty, YouTube/Spotify disabled if unavailable
        bool is_disabled = (i == 0 && resume_count == 0) ||
                          (i == 2 && favorites_count == 0) ||
                          (i == 3 && !yt_available) ||
                          (i == 4 && !sp_available);

        // Selection box
        if (is_selected) {
            draw_retro_box(box_x, y, box_width, box_height, 3, COLOR_HIGHLIGHT, COLOR_DIM);
        } else {
            draw_retro_box(box_x, y, box_width, box_height, 2, COLOR_BG, COLOR_DIM);
        }

        // Label with count
        char display[64];
        if (counts[i] >= 0) {
            snprintf(display, sizeof(display), "%s (%d)", labels[i], counts[i]);
        } else {
            snprintf(display, sizeof(display), "%s", labels[i]);
        }

        SDL_Color color = is_disabled ? COLOR_DIM : (is_selected ? COLOR_ACCENT : COLOR_TEXT);
        int text_h;
        TTF_SizeUTF8(g_font_medium, display, NULL, &text_h);
        render_text_centered(display, y + (box_height - text_h) / 2, g_font_medium, color);
    }

    // Dancing monkey below menu items
    // Footer controls
    char home_hint[96];
    snprintf(home_hint, sizeof(home_hint), "%s: Select   %s: Options   %s: Help", BTN_A, BTN_START, BTN_X);
    render_text(home_hint, MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_resume(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Resume", COLOR_TEXT, false);

    // Status bar (volume + battery) in top right
    render_status_bar();

    // Draw header separator
    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // Get state
    int count = positions_get_count();
    int cursor = resume_get_cursor();
    int scroll = resume_get_scroll();

    // List
    int y = HEADER_HEIGHT + 8;
    int max_width = g_screen_width - MARGIN * 4;

    // Color for position indicator
    SDL_Color COLOR_POSITION = {255, 160, 0, 255};  // Orange

    if (count == 0) {
        // Empty state with monkey
        render_text_centered("No saved positions", g_screen_height / 2 - 60, g_font_medium, COLOR_DIM);
        render_monkey(g_screen_width / 2 - 48, g_screen_height / 2, false);
    } else {
        for (int i = 0; i < HOME_LIST_VISIBLE && (scroll + i) < count; i++) {
            int idx = scroll + i;
            bool is_selected = (idx == cursor);

            // Get entry
            char path[512];
            int pos_sec = positions_get_entry(idx, path, sizeof(path));
            if (pos_sec < 0) continue;

            // Extract filename
            const char *filename = strrchr(path, '/');
            filename = filename ? filename + 1 : path;

            // Selection highlight
            if (is_selected) {
                draw_retro_box(0, y - 6, g_screen_width, LINE_HEIGHT, 3, COLOR_HIGHLIGHT, COLOR_DIM);
            }

            // Build display text with position
            char display[300];
            int mins = pos_sec / 60;
            int secs = pos_sec % 60;
            snprintf(display, sizeof(display), "> %s", filename);

            // Render filename
            SDL_Color color = is_selected ? COLOR_ACCENT : COLOR_TEXT;
            render_text_truncated(display, MARGIN, y, max_width - 100, g_font_medium, color);

            // Render position on right side
            char pos_str[16];
            snprintf(pos_str, sizeof(pos_str), "[%d:%02d]", mins, secs);
            render_text(pos_str, g_screen_width - 150, y, g_font_small, COLOR_POSITION);

            y += LINE_HEIGHT;
        }
    }

    // Footer with scroll indicator
    if (count > HOME_LIST_VISIBLE) {
        draw_rect(0, g_screen_height - FOOTER_HEIGHT, g_screen_width, 2, COLOR_DIM);

        char scroll_info[32];
        snprintf(scroll_info, sizeof(scroll_info), "%d/%d", cursor + 1, count);
        render_text(scroll_info, g_screen_width - 120 - SCREEN_PAD, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
    }

    // Controls hint
    render_text(BTN_A ":Play  " BTN_Y ":Remove  " BTN_B ":Back", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_favorites(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Favorites", COLOR_TEXT, false);

    // Status bar (volume + battery) in top right
    render_status_bar();

    // Draw header separator
    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // Get state
    int count = favorites_get_count();
    int cursor = favorites_get_cursor();
    int scroll = favorites_get_scroll();

    // List
    int y = HEADER_HEIGHT + 8;
    int max_width = g_screen_width - MARGIN * 4;

    if (count == 0) {
        // Empty state with monkey
        render_text_centered("No favorites yet", g_screen_height / 2 - 60, g_font_medium, COLOR_DIM);
        render_monkey(g_screen_width / 2 - 48, g_screen_height / 2, false);
    } else {
        for (int i = 0; i < HOME_LIST_VISIBLE && (scroll + i) < count; i++) {
            int idx = scroll + i;
            bool is_selected = (idx == cursor);

            // Get entry
            const char *path = favorites_get_path(idx);
            if (!path) continue;

            // Extract filename
            const char *filename = strrchr(path, '/');
            filename = filename ? filename + 1 : path;

            // Check if has saved position
            int pos_sec = positions_get(path);

            // Selection highlight
            if (is_selected) {
                draw_retro_box(0, y - 6, g_screen_width, LINE_HEIGHT, 3, COLOR_HIGHLIGHT, COLOR_DIM);
            }

            // Build display text
            char display[300];
            snprintf(display, sizeof(display), "* %s", filename);

            // Render filename
            SDL_Color color = is_selected ? COLOR_ACCENT : COLOR_TEXT;
            render_text_truncated(display, MARGIN, y, max_width - 100, g_font_medium, color);

            // Render position if available
            if (pos_sec > 0) {
                SDL_Color COLOR_POSITION = {255, 160, 0, 255};
                char pos_str[16];
                snprintf(pos_str, sizeof(pos_str), "[%d:%02d]", pos_sec / 60, pos_sec % 60);
                render_text(pos_str, g_screen_width - 150, y, g_font_small, COLOR_POSITION);
            }

            y += LINE_HEIGHT;
        }
    }

    // Footer with scroll indicator
    if (count > HOME_LIST_VISIBLE) {
        draw_rect(0, g_screen_height - FOOTER_HEIGHT, g_screen_width, 2, COLOR_DIM);

        char scroll_info[32];
        snprintf(scroll_info, sizeof(scroll_info), "%d/%d", cursor + 1, count);
        render_text(scroll_info, g_screen_width - 120 - SCREEN_PAD, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
    }

    // Controls hint
    render_text(BTN_A ":Play  " BTN_Y ":Remove  " BTN_B ":Back", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_browser(void) {
    // Periodically expire old cache entries
    static Uint32 last_cache_expire = 0;
    Uint32 now = SDL_GetTicks();
    if (now - last_cache_expire > 1000) {  // Check every second
        text_cache_expire();
        last_cache_expire = now;
    }

    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Explorer", COLOR_TEXT, false);

    // Download indicator (next to header, grows right)
    int pending = dlqueue_pending_count();
    if (pending > 0) {
        char dl_str[32];
        int progress = dlqueue_get_progress();
        if (progress >= 0) {
            snprintf(dl_str, sizeof(dl_str), "DL:%d%% (%d)", progress, pending);
        } else {
            snprintf(dl_str, sizeof(dl_str), "DL:(%d)", pending);
        }
        render_text(dl_str, get_header_end_x("> Explorer") + 4, 16, g_font_small, COLOR_ACCENT);
    }

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

        // Format indicator (right-aligned, files only)
        if (entry->type == ENTRY_FILE) {
            const char *fmt = audio_format_from_path(entry->full_path);
            if (fmt[0]) {
                int fmt_w;
                TTF_SizeUTF8(g_font_small, fmt, &fmt_w, NULL);
                render_text(fmt, g_screen_width - MARGIN * 2 - fmt_w, y + 4, g_font_small, COLOR_DIM);
            }
        }

        y += LINE_HEIGHT;
    }

    // Footer with scroll indicator
    if (count > VISIBLE_ITEMS) {
        draw_rect(0, g_screen_height - FOOTER_HEIGHT, g_screen_width, 2, COLOR_DIM);

        char scroll_info[32];
        snprintf(scroll_info, sizeof(scroll_info), "%d/%d", cursor + 1, count);
        render_text(scroll_info, g_screen_width - 120 - SCREEN_PAD, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
    }

    // Empty directory message
    if (count == 0) {
        render_text_centered("No music files found", g_screen_height / 2, g_font_medium, COLOR_DIM);
    }

    // Controls hint
#ifdef __APPLE__
    render_text(BTN_A ":Open  " BTN_Y ":Fav  " BTN_B ":Up  H:Help", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
#else
    render_text(BTN_A ":Open  " BTN_Y ":Fav  " BTN_B ":Up  " BTN_X ":Help", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
#endif

    render_version_watermark();
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

/**
 * Render text with horizontal scroll if it exceeds max_width
 * @param text The text to render
 * @param y Y position
 * @param max_width Maximum width in pixels before scrolling
 * @param font Font to use
 * @param color Text color
 * @param scroll_offset Pointer to scroll state variable
 * @param has_shadow Whether to render with shadow (for cover bg)
 */
static void render_scrolling_text_centered(const char *text, int y, int max_width,
                                           TTF_Font *font, SDL_Color color,
                                           int *scroll_offset, bool has_shadow) {
    if (!text || !text[0]) return;

    int text_width, text_height;
    TTF_SizeUTF8(font, text, &text_width, &text_height);

    if (text_width <= max_width) {
        // Fits on screen - render centered normally
        if (has_shadow) {
            render_text_centered_shadow(text, y, font, color);
        } else {
            render_text_centered(text, y, font, color);
        }
        *scroll_offset = 0;
        return;
    }

    // Text is too long - scroll horizontally
    Uint32 now = SDL_GetTicks();
    if (now > g_player_scroll_last_update &&
        now - g_player_scroll_last_update > PLAYER_SCROLL_SPEED_MS) {
        (*scroll_offset)++;
        g_player_scroll_last_update = now;
    }

    // Create extended text with gap for seamless loop
    char extended[512];
    snprintf(extended, sizeof(extended), "%s%s%s", text, PLAYER_SCROLL_GAP, text);

    int gap_len = strlen(PLAYER_SCROLL_GAP);
    int text_len = strlen(text);
    int cycle_len = text_len + gap_len;

    // Reset at end of cycle (with pause)
    if (*scroll_offset >= cycle_len) {
        *scroll_offset = 0;
        g_player_scroll_last_update = now + PLAYER_SCROLL_PAUSE_MS;
    }

    // Calculate visible portion based on character offset
    // Use byte-safe substring starting at scroll_offset
    int start = *scroll_offset;
    if (start >= (int)strlen(extended)) start = 0;

    // Find how many chars fit in max_width
    char visible[256];
    int visible_len = 0;
    const char *src = extended + start;

    while (*src && visible_len < (int)sizeof(visible) - 1) {
        // Add one character at a time and check width
        visible[visible_len] = *src;
        visible[visible_len + 1] = '\0';

        int new_width;
        TTF_SizeUTF8(font, visible, &new_width, NULL);

        if (new_width > max_width) {
            // This char would overflow - stop here
            visible[visible_len] = '\0';
            break;
        }

        visible_len++;
        src++;
    }

    // Render left-aligned with margin (scroll effect)
    int x = MARGIN;
    if (has_shadow) {
        render_text_shadow(visible, x, y, font, color);
    } else {
        render_text(visible, x, y, font, color);
    }
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
        render_header_shadow("> Player", cover_text, audio_is_playing());
    } else {
        render_header("> Player", COLOR_TEXT, audio_is_playing());
    }

    // Favorite indicator if current track is a favorite
    const char *current_path = browser_get_selected_path();
    int fav_indicator_x = get_header_end_x("> Player");
    if (current_path && favorites_is_favorite(current_path)) {
        if (has_cover_bg) {
            render_text_shadow("*", fav_indicator_x, 8, g_font_medium, cover_accent);
        } else {
            render_text("*", fav_indicator_x, 8, g_font_medium, COLOR_ACCENT);
        }
        fav_indicator_x += 24;
    }

    // Favorites playback mode indicator (shows that NEXT/PREV navigates favorites)
    if (favorites_is_playback_mode()) {
        if (has_cover_bg) {
            render_text_shadow("[FAV]", fav_indicator_x, 8, g_font_small, cover_accent);
        } else {
            render_text("[FAV]", fav_indicator_x, 8, g_font_small, COLOR_ACCENT);
        }
    }

    // Download indicator (next to header, grows right)
    int pending = dlqueue_pending_count();
    if (pending > 0) {
        char dl_str[32];
        int progress = dlqueue_get_progress();
        if (progress >= 0) {
            snprintf(dl_str, sizeof(dl_str), "DL:%d%% (%d)", progress, pending);
        } else {
            snprintf(dl_str, sizeof(dl_str), "DL:(%d)", pending);
        }
        int dl_x = fav_indicator_x + 12;
        if (has_cover_bg) {
            render_text_shadow(dl_str, dl_x, 16, g_font_small, cover_accent);
        } else {
            render_text(dl_str, dl_x, 16, g_font_small, COLOR_ACCENT);
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

    // Detect song change and reset scroll
    if (strcmp(info->title, g_player_last_title) != 0) {
        g_player_title_scroll = 0;
        g_player_artist_scroll = 0;
        g_player_scroll_last_update = SDL_GetTicks() + PLAYER_SCROLL_PAUSE_MS;
        strncpy(g_player_last_title, info->title, sizeof(g_player_last_title) - 1);
        g_player_last_title[sizeof(g_player_last_title) - 1] = '\0';
    }

    // Max width for title/artist (screen width minus margins)
    int max_text_width = g_screen_width - MARGIN * 2;

    // Title and Artist (with scroll for long text, shadow when cover bg)
    if (has_cover_bg) {
        render_scrolling_text_centered(info->title, center_y, max_text_width,
                                       g_font_large, cover_text,
                                       &g_player_title_scroll, true);
        render_scrolling_text_centered(info->artist, center_y + 90, max_text_width,
                                       g_font_medium, cover_text,
                                       &g_player_artist_scroll, true);
    } else {
        render_scrolling_text_centered(info->title, center_y, max_text_width,
                                       g_font_large, COLOR_TEXT,
                                       &g_player_title_scroll, false);
        render_scrolling_text_centered(info->artist, center_y + 90, max_text_width,
                                       g_font_medium, COLOR_DIM,
                                       &g_player_artist_scroll, false);
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

    // Format badge (right-aligned on time line)
    const char *fmt = audio_get_format_string();
    if (fmt[0]) {
        SDL_Color fmt_color = has_cover_bg ? cover_accent : COLOR_ACCENT;
        int fmt_x = bar_x + bar_w;
        int fmt_w;
        TTF_SizeUTF8(g_font_small, fmt, &fmt_w, NULL);
        fmt_x -= fmt_w;
        if (has_cover_bg) {
            render_text_shadow(fmt, fmt_x, bar_y + 24, g_font_small, fmt_color);
        } else {
            render_text(fmt, fmt_x, bar_y + 24, g_font_small, fmt_color);
        }
    }

    // Footer with controls (only separator line without cover bg)
    if (!has_cover_bg) {
        draw_rect(0, g_screen_height - FOOTER_HEIGHT, g_screen_width, 2, COLOR_DIM);
    }

    // Control icons (with shadow when cover background)
    // Equal spacing: 150px between each item
    int ctrl_y = g_screen_height - SCREEN_PAD - 32;
    int ctrl_spacing = 150;
    int ctrl_x = MARGIN;
    SDL_Color ctrl_color = has_cover_bg ? cover_text : COLOR_DIM;
    if (has_cover_bg) {
        render_text_shadow("L:Prev", ctrl_x, ctrl_y, g_font_small, ctrl_color);
        render_text_shadow(BTN_A ":Play", ctrl_x + ctrl_spacing, ctrl_y, g_font_small, ctrl_color);
        render_text_shadow("R:Next", ctrl_x + ctrl_spacing * 2, ctrl_y, g_font_small, ctrl_color);
        render_text_shadow(BTN_B ":Back", ctrl_x + ctrl_spacing * 3, ctrl_y, g_font_small, ctrl_color);
#ifdef __APPLE__
        render_text_shadow("H:Help", ctrl_x + ctrl_spacing * 4, ctrl_y, g_font_small, ctrl_color);
#else
        render_text_shadow(BTN_X ":Help", ctrl_x + ctrl_spacing * 4, ctrl_y, g_font_small, ctrl_color);
#endif
    } else {
        render_text("L:Prev", ctrl_x, ctrl_y, g_font_small, ctrl_color);
        render_text(BTN_A ":Play", ctrl_x + ctrl_spacing, ctrl_y, g_font_small, ctrl_color);
        render_text("R:Next", ctrl_x + ctrl_spacing * 2, ctrl_y, g_font_small, ctrl_color);
        render_text(BTN_B ":Back", ctrl_x + ctrl_spacing * 3, ctrl_y, g_font_small, ctrl_color);
#ifdef __APPLE__
        render_text("H:Help", ctrl_x + ctrl_spacing * 4, ctrl_y, g_font_small, ctrl_color);
#else
        render_text(BTN_X ":Help", ctrl_x + ctrl_spacing * 4, ctrl_y, g_font_small, ctrl_color);
#endif
    }
}

void ui_render_player(void) {
    ui_render_player_content();
    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_menu(void) {
    // Render appropriate background based on menu mode
    // Player content behind when opened from player, solid bg otherwise
    int item_count = menu_get_item_count();
    if (item_count == 4) {
        // Player mode - render player underneath
        ui_render_player_content();
    } else {
        // Browser/Home mode - solid background
        SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(g_renderer);
    }

    // Darken overlay
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 180);
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);

    // Menu box - height adapts to item count
    int menu_w = 500;
    int item_h = 46;
    int title_space = 80;
    int footer_space = 50;
    int menu_h = title_space + (item_count * item_h) + footer_space;
    int menu_x = (g_screen_width - menu_w) / 2;
    int menu_y = (g_screen_height - menu_h) / 2;

    // Draw retro box for menu
    draw_retro_box(menu_x, menu_y, menu_w, menu_h, 4, COLOR_HIGHLIGHT, COLOR_TEXT);

    // Menu title
    render_text_centered("Options", menu_y + 20, g_font_medium, COLOR_TEXT);

    // Menu items (dynamic from menu system)
    int cursor = menu_get_cursor();
    int item_y = menu_y + title_space;

    for (int i = 0; i < item_count; i++) {
        const char *label = menu_get_item_label(i);
        if (cursor == i) {
            draw_rect(menu_x + 16, item_y - 4, menu_w - 32, item_h, COLOR_ACCENT);
            render_text(label, menu_x + 32, item_y, g_font_small, COLOR_BG);
        } else {
            render_text(label, menu_x + 32, item_y, g_font_small, COLOR_DIM);
        }
        item_y += item_h;
    }

    // Controls hint
    render_text_centered(BTN_A ":Select  " BTN_B ":Close", menu_y + menu_h - 40, g_font_small, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

// External: EQ band selection from main.c
extern int eq_get_selected_band(void);

void ui_render_equalizer(void) {
    // Render player underneath (EQ only accessible from player menu)
    ui_render_player_content();

    // Darken overlay
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 200);
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);

    // EQ box dimensions
    int box_w = 740;
    int box_h = 480;
    int box_x = (g_screen_width - box_w) / 2;
    int box_y = (g_screen_height - box_h) / 2;

    draw_retro_box(box_x, box_y, box_w, box_h, 4, COLOR_HIGHLIGHT, COLOR_TEXT);

    // Title
    render_text_centered("Equalizer", box_y + 16, g_font_medium, COLOR_TEXT);

    int selected = eq_get_selected_band();
    int band_count = eq_get_band_count();

    // Vertical bar layout - 5 columns evenly spaced
    int bar_area_x = box_x + 60;
    int bar_area_w = box_w - 120;
    int col_w = bar_area_w / band_count;
    int bar_w = 40;
    int bar_max_h = 240;
    int bar_top = box_y + 80;
    int bar_center_y = bar_top + bar_max_h / 2;  // Zero line

    for (int i = 0; i < band_count; i++) {
        int cx = bar_area_x + col_w * i + col_w / 2;  // Column center x
        int bx = cx - bar_w / 2;  // Bar left x

        int db = eq_get_band_db(i);
        SDL_Color label_color = (i == selected) ? COLOR_TEXT : COLOR_DIM;
        SDL_Color bar_color = (i == selected) ? COLOR_ACCENT : COLOR_DIM;

        // Bar background (full height, dimmed)
        SDL_SetRenderDrawColor(g_renderer, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, 50);
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_Rect bar_bg = {bx, bar_top, bar_w, bar_max_h};
        SDL_RenderFillRect(g_renderer, &bar_bg);

        // Zero line marker
        SDL_SetRenderDrawColor(g_renderer, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, 120);
        SDL_RenderDrawLine(g_renderer, bx - 4, bar_center_y, bx + bar_w + 4, bar_center_y);

        // Filled portion: grows up from center for positive, down for negative
        if (db != 0) {
            float ratio = (float)abs(db) / (float)(-EQ_MIN_DB);  // 0.0 to 1.0
            int fill_h = (int)(ratio * (bar_max_h / 2));

            SDL_SetRenderDrawColor(g_renderer, bar_color.r, bar_color.g, bar_color.b, 255);
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);

            if (db > 0) {
                // Grow upward from center
                SDL_Rect fill = {bx, bar_center_y - fill_h, bar_w, fill_h};
                SDL_RenderFillRect(g_renderer, &fill);
            } else {
                // Grow downward from center
                SDL_Rect fill = {bx, bar_center_y, bar_w, fill_h};
                SDL_RenderFillRect(g_renderer, &fill);
            }
        }

        // Bar outline
        SDL_SetRenderDrawColor(g_renderer, label_color.r, label_color.g, label_color.b,
                              (i == selected) ? 255 : 100);
        SDL_SetRenderDrawBlendMode(g_renderer, (i == selected) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
        SDL_RenderDrawRect(g_renderer, &bar_bg);

        // Selection indicator (triangle/arrow below bar)
        if (i == selected) {
            int ty = bar_top + bar_max_h + 8;
            SDL_SetRenderDrawColor(g_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
            // Small triangle pointing up
            for (int row = 0; row < 6; row++) {
                SDL_RenderDrawLine(g_renderer, cx - row, ty + row, cx + row, ty + row);
            }
        }

        // Frequency label below
        int label_y = bar_top + bar_max_h + 20;
        const char *label = eq_get_band_label(i);
        // Center label text under bar
        int tw = 0, th = 0;
        TTF_SizeText(g_font_small, label, &tw, &th);
        render_text(label, cx - tw / 2, label_y, g_font_small, label_color);

        // dB value above bar (positioned per column)
        const char *db_str = eq_get_band_string(i);
        TTF_SizeText(g_font_small, db_str, &tw, &th);
        render_text(db_str, cx - tw / 2, bar_top - 36, g_font_small,
                   (i == selected) ? COLOR_TEXT : COLOR_DIM);
    }

    // Footer hints
    render_text(BTN_A ":Reset", box_x + 40, box_y + box_h - 44, g_font_small, COLOR_DIM);
    render_text(BTN_B ":Back", box_x + box_w - 160, box_y + box_h - 44, g_font_small, COLOR_DIM);

    render_version_watermark();
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
    render_text_centered(BTN_X ":Close", box_y + box_h - 60, g_font_small, COLOR_DIM);
#endif

    render_version_watermark();
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
        "L2         Jump to start",
        "R2         Jump near end",
        "Y          Toggle favorite",
        "Select     Dim screen",
        "Start      Options menu",
        "Start+B    Exit app"
    };

    render_help_overlay("Now Playing", lines, 11);
}

void ui_render_loading(const char *filename) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Loading", COLOR_TEXT, false);

    // Loading message (centered)
    render_text_centered("Loading...", g_screen_height / 2 - 40, g_font_large, COLOR_ACCENT);

    // Filename (centered, below loading)
    if (filename && filename[0]) {
        render_text_centered(filename, g_screen_height / 2 + 40, g_font_medium, COLOR_DIM);
    }

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_scanning(int current, int total, const char *current_file, int found) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Scanner", COLOR_TEXT, false);

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
    render_text_centered(BTN_B ": Cancel", g_screen_height - 60, g_font_small, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_scan_complete(int found, int total) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Scanner", COLOR_TEXT, false);

    // Title
    render_text_centered("Scan Complete!", g_screen_height / 2 - 60, g_font_large, COLOR_ACCENT);

    // Results
    char results[128];
    snprintf(results, sizeof(results), "Found metadata for %d of %d files", found, total);
    render_text_centered(results, g_screen_height / 2 + 20, g_font_medium, COLOR_TEXT);

    // Hint
    render_text_centered("Press any button to continue", g_screen_height - 100, g_font_small, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_error(const char *message) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Error", COLOR_TEXT, false);

    // Error icon/text (centered)
    render_text_centered("Error", g_screen_height / 2 - 60, g_font_large, COLOR_ERROR);

    // Error message (centered, below error)
    if (message && message[0]) {
        render_text_centered(message, g_screen_height / 2 + 20, g_font_medium, COLOR_DIM);
    }

    // Hint
    render_text_centered("Press any button to continue", g_screen_height - 100, g_font_small, COLOR_DIM);

    render_version_watermark();
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
    render_text_centered(BTN_A ":Select  " BTN_B ":Cancel", box_y + box_h - 60, g_font_small, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_confirm_delete(void) {
    // Clear screen completely (don't rely on previous render)
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Dark background overlay
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 180);
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);

    // Calculate dynamic box size
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

    // Warning title
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
    render_text_centered(BTN_A ":Confirm  " BTN_B ":Cancel", box_y + box_h - 50, g_font_small, COLOR_DIM);

    // Reset blend mode and present
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_resume_prompt(int saved_pos) {
    // Clear screen completely (don't rely on previous render)
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Dark background overlay
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 180);
    SDL_Rect overlay = {0, 0, g_screen_width, g_screen_height};
    SDL_RenderFillRect(g_renderer, &overlay);

    // Calculate box size
    int box_w = 650;
    int box_h = 220;
    int box_x = (g_screen_width - box_w) / 2;
    int box_y = (g_screen_height - box_h) / 2;

    draw_rect(box_x, box_y, box_w, box_h, COLOR_HIGHLIGHT);

    // Title
    render_text_centered("Resume Playback?", box_y + 30, g_font_large, COLOR_ACCENT);

    // Format saved position as MM:SS
    int mins = saved_pos / 60;
    int secs = saved_pos % 60;
    char position_text[64];
    snprintf(position_text, sizeof(position_text), "Continue from %d:%02d", mins, secs);
    render_text_centered(position_text, box_y + 100, g_font_medium, COLOR_TEXT);

    // Controls hint
    render_text_centered(BTN_A ":Resume  " BTN_B ":Start Over", box_y + box_h - 50, g_font_small, COLOR_DIM);

    // Reset blend mode and present
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
    render_version_watermark();
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
    render_text_centered("D-Pad: Move   " BTN_A ": Insert   " BTN_B ": Delete", ctrl_y, g_font_small, COLOR_DIM);
    render_text_centered(BTN_START ": Confirm   " BTN_SELECT ": Cancel", ctrl_y + 35, g_font_small, COLOR_DIM);

    render_version_watermark();
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

        render_version_watermark();
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
    render_text_centered("D-Pad: Move   " BTN_A ": Insert   " BTN_B ": Delete", ctrl_y, g_font_small, COLOR_DIM);
    render_text_centered(BTN_START ": Search   " BTN_SELECT ": Cancel", ctrl_y + 30, g_font_small, COLOR_DIM);

    render_version_watermark();
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

        // Check if already in queue
        bool is_queued = dlqueue_is_queued(result->id);

        // Render queue indicator
        if (is_queued) {
            render_text("+", MARGIN - 5, y, g_font_small, COLOR_ACCENT);
        }

        // Render title (smaller font)
        render_text(title_display, MARGIN + 10, y,
                   g_font_small, is_selected ? COLOR_ACCENT : (is_queued ? COLOR_DIM : COLOR_TEXT));

        // Render channel and duration on next line
        char meta_display[128];
        snprintf(meta_display, sizeof(meta_display), "%.35s  [%s]%s",
                 result->channel, duration_str, is_queued ? " (queued)" : "");
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

    // Controls hint - update to reflect queue behavior
    int pending = dlqueue_pending_count();
    if (pending > 0) {
        char hint[64];
        snprintf(hint, sizeof(hint), "%s:Add (%d)  %s:Queue  %s:Back", BTN_A, pending, BTN_X, BTN_B);
        render_text(hint, MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
    } else {
        render_text(BTN_A ":Add to queue  " BTN_X ":View queue  " BTN_B ":Back", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
    }

    // Render toast overlay before present
    render_toast_overlay();

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_youtube_download(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> YouTube", COLOR_TEXT, false);

    // Title "DOWNLOADING"
    render_text_centered("DOWNLOADING", 80, g_font_large, COLOR_ACCENT);

    // Dancing monkey below title
    int monkey_x = (g_screen_width - 16 * MONKEY_PIXEL_SIZE) / 2;
    int monkey_y = 140;
    render_monkey(monkey_x, monkey_y, true);  // true = dancing animation

    // Track title (100px below monkey area)
    const char *title = ytsearch_get_download_title();
    int title_y = monkey_y + 16 * MONKEY_PIXEL_SIZE + 40;  // After monkey + spacing
    if (title) {
        char title_display[64];
        if (strlen(title) > 40) {
            snprintf(title_display, sizeof(title_display), "%.37s...", title);
        } else {
            strncpy(title_display, title, sizeof(title_display) - 1);
            title_display[sizeof(title_display) - 1] = '\0';
        }
        render_text_centered(title_display, title_y, g_font_medium, COLOR_TEXT);
    }

    // Progress bar
    int bar_w = 500;
    int bar_h = 30;
    int bar_x = (g_screen_width - bar_w) / 2;
    int bar_y = title_y + 60;

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

    // Error message (if any) - shows exit code
    const char *error = ytsearch_get_error();
    if (error) {
        render_text_centered(error, bar_y + bar_h + 110, g_font_small, COLOR_ERROR);
    }

    // Cancel hint
    render_text_centered(BTN_B ": Cancel", g_screen_height - 60, g_font_small, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_download_queue(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    int pending = dlqueue_pending_count();
    int total = dlqueue_total_count();
    char header[128];
    snprintf(header, sizeof(header), "Download Queue (%d pending, %d total)", pending, total);
    render_text(header, MARGIN, 8, g_font_small, COLOR_TEXT);

    // Empty state
    if (total == 0) {
        render_text_centered("Queue is empty", g_screen_height / 2 - 40, g_font_medium, COLOR_DIM);
        render_text_centered("Press A on search results to add downloads", g_screen_height / 2 + 20, g_font_small, COLOR_DIM);
        render_text_centered(BTN_B ": Back", g_screen_height - 60, g_font_small, COLOR_DIM);
        render_version_watermark();
        SDL_RenderPresent(g_renderer);
        return;
    }

    // Queue list
    int cursor = dlqueue_view_get_cursor();
    int scroll = dlqueue_view_get_scroll_offset();
    int visible = 8;
    int y = HEADER_HEIGHT + 10;

    for (int i = 0; i < visible && (scroll + i) < total; i++) {
        int idx = scroll + i;
        const DownloadItem *item = dlqueue_get_item(idx);
        if (!item) continue;

        bool is_selected = (idx == cursor);

        // Highlight selected item
        if (is_selected) {
            draw_rect(0, y - 5, g_screen_width, LINE_HEIGHT + 10, COLOR_HIGHLIGHT);
        }

        // Status icon
        const char *status_icon;
        SDL_Color status_color;
        switch (item->status) {
            case DL_PENDING:
                status_icon = "[...]";
                status_color = COLOR_DIM;
                break;
            case DL_DOWNLOADING:
                status_icon = "[>>]";
                status_color = COLOR_ACCENT;
                break;
            case DL_COMPLETE:
                status_icon = "[OK]";
                status_color = COLOR_ACCENT;
                break;
            case DL_FAILED:
                status_icon = "[X]";
                status_color = COLOR_ERROR;
                break;
            default:
                status_icon = "[?]";
                status_color = COLOR_DIM;
                break;
        }

        render_text(status_icon, MARGIN, y, g_font_small, status_color);

        // Title (truncated)
        char title_display[64];
        if (strlen(item->title) > 45) {
            snprintf(title_display, sizeof(title_display), "%.42s...", item->title);
        } else {
            strncpy(title_display, item->title, sizeof(title_display) - 1);
            title_display[sizeof(title_display) - 1] = '\0';
        }

        render_text(title_display, MARGIN + 70, y, g_font_small,
                   is_selected ? COLOR_ACCENT : COLOR_TEXT);

        // Progress bar for downloading items
        if (item->status == DL_DOWNLOADING) {
            int bar_x = g_screen_width - MARGIN - 150;
            int bar_w = 130;
            int bar_h = 16;
            int bar_y_offset = y + 4;

            // Background
            draw_rect(bar_x, bar_y_offset, bar_w, bar_h, COLOR_DIM);

            // Fill
            if (item->progress > 0) {
                int fill_w = (bar_w - 4) * item->progress / 100;
                draw_rect(bar_x + 2, bar_y_offset + 2, fill_w, bar_h - 4, COLOR_ACCENT);
            }

            // Percentage text
            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", item->progress);
            render_text(pct, bar_x + bar_w + 5, y, g_font_small, COLOR_DIM);
        }

        // Channel name on second line
        char meta_display[128];
        if (item->status == DL_FAILED && item->error[0]) {
            snprintf(meta_display, sizeof(meta_display), "%.30s - Error: %.30s",
                     item->channel, item->error);
        } else {
            snprintf(meta_display, sizeof(meta_display), "%.50s", item->channel);
        }
        render_text(meta_display, MARGIN + 80, y + 28, g_font_small, COLOR_DIM);

        y += LINE_HEIGHT + 10;
    }

    // Scroll indicators
    if (scroll > 0) {
        render_text_centered("^ more ^", HEADER_HEIGHT - 5, g_font_small, COLOR_DIM);
    }
    if (scroll + visible < total) {
        render_text_centered("v more v", g_screen_height - 90, g_font_small, COLOR_DIM);
    }

    // Footer - cursor position
    char footer[64];
    snprintf(footer, sizeof(footer), "%d of %d", cursor + 1, total);
    render_text_centered(footer, g_screen_height - 65, g_font_small, COLOR_DIM);

    // Controls hint
    render_text(BTN_A ":Play  " BTN_Y ":Clear completed  " BTN_X ":Cancel  " BTN_B ":Back",
               MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);

    // Render toast overlay before present
    render_toast_overlay();

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

/**
 * Internal: Render toast overlay (call before SDL_RenderPresent)
 */
static void render_toast_overlay(void) {
    if (g_toast_message[0] == '\0') return;

    Uint32 now = SDL_GetTicks();
    Uint32 elapsed_ms = now - g_toast_start_time;

    if (elapsed_ms >= UI_TOAST_DURATION_MS) {
        g_toast_message[0] = '\0';  // Clear expired toast
        return;
    }

    // Fade out in last 500ms
    #define TOAST_FADE_MS 500

    int alpha = 255;
    if (elapsed_ms > UI_TOAST_DURATION_MS - TOAST_FADE_MS) {
        alpha = 255 * (UI_TOAST_DURATION_MS - elapsed_ms) / TOAST_FADE_MS;
        if (alpha < 0) alpha = 0;
    }

    // Calculate text width for background box
    int text_w, text_h;
    TTF_SizeUTF8(g_font_small, g_toast_message, &text_w, &text_h);

    int box_w = text_w + 40;
    int box_h = text_h + 20;
    int box_x = (g_screen_width - box_w) / 2;
    int box_y = g_screen_height - 120;

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 40, 40, 40, (Uint8)(alpha * 0.9));
    SDL_Rect bg_rect = {box_x, box_y, box_w, box_h};
    SDL_RenderFillRect(g_renderer, &bg_rect);

    // Border
    SDL_SetRenderDrawColor(g_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, (Uint8)alpha);
    SDL_RenderDrawRect(g_renderer, &bg_rect);

    // Text (centered in box)
    SDL_Color text_color = {COLOR_TEXT.r, COLOR_TEXT.g, COLOR_TEXT.b, (Uint8)alpha};
    SDL_Surface *surface = TTF_RenderUTF8_Blended(g_font_small, g_toast_message, text_color);
    if (surface) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, surface);
        if (texture) {
            SDL_SetTextureAlphaMod(texture, (Uint8)alpha);
            SDL_Rect dest = {
                box_x + (box_w - text_w) / 2,
                box_y + (box_h - text_h) / 2,
                text_w,
                text_h
            };
            SDL_RenderCopy(g_renderer, texture, NULL, &dest);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
}

/**
 * Internal: Render version watermark (bottom-right corner)
 */
static void render_version_watermark(void) {
    char version_str[32];
    snprintf(version_str, sizeof(version_str), "v%s", VERSION);

    // Render small, dimmed text in bottom-right corner
    int text_w, text_h;
    TTF_SizeUTF8(g_font_tiny, version_str, &text_w, &text_h);

    int x = g_screen_width - text_w - SCREEN_PAD;
    int y = g_screen_height - text_h - SCREEN_PAD;

    // Very dim color (like a watermark)
    SDL_Color watermark_color = {COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, 128};

    SDL_Surface *surface = TTF_RenderUTF8_Blended(g_font_tiny, version_str, watermark_color);
    if (surface) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, surface);
        if (texture) {
            SDL_SetTextureAlphaMod(texture, 128);  // 50% opacity
            SDL_Rect dest = {x, y, text_w, text_h};
            SDL_RenderCopy(g_renderer, texture, NULL, &dest);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
}

void ui_show_toast(const char *message) {
    if (!message) {
        g_toast_message[0] = '\0';
        return;
    }
    strncpy(g_toast_message, message, sizeof(g_toast_message) - 1);
    g_toast_message[sizeof(g_toast_message) - 1] = '\0';
    g_toast_start_time = SDL_GetTicks();
}

bool ui_toast_active(void) {
    if (g_toast_message[0] == '\0') return false;
    return (SDL_GetTicks() - g_toast_start_time) < UI_TOAST_DURATION_MS;
}

const char* ui_get_toast_message(void) {
    return g_toast_message[0] ? g_toast_message : NULL;
}

void ui_player_reset_scroll(void) {
    g_player_title_scroll = 0;
    g_player_artist_scroll = 0;
    g_player_scroll_last_update = SDL_GetTicks() + PLAYER_SCROLL_PAUSE_MS;
    g_player_last_title[0] = '\0';
}

// ============================================================================
// Spotify UI Render Functions
// ============================================================================

void ui_render_spotify_connect(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_text("> Spotify Connect", MARGIN, 8, g_font_medium, COLOR_ACCENT);
    render_status_bar();
    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // Main instruction text
    render_text_centered("Open Spotify on your phone", g_screen_height / 2 - 120, g_font_medium, COLOR_TEXT);
    render_text_centered("and select 'Mono'", g_screen_height / 2 - 70, g_font_medium, COLOR_TEXT);

    // Animated monkey (always animating while waiting)
    int monkey_x = (g_screen_width - 16 * MONKEY_PIXEL_SIZE) / 2;
    int monkey_y = g_screen_height / 2 + 10;
    render_monkey(monkey_x, monkey_y, true);

    // Status message
    SpotifyState sp_state = spotify_get_state();
    const char *status_msg = "Waiting for connection...";
    SDL_Color status_color = COLOR_DIM;

    if (sp_state == SP_STATE_CONNECTED) {
        status_msg = "Connected!";
        status_color = COLOR_ACCENT;
    } else if (sp_state == SP_STATE_ERROR) {
        const char *err = spotify_get_error();
        status_msg = err ? err : "Connection error";
        status_color = COLOR_ERROR;
    }

    render_text_centered(status_msg, g_screen_height / 2 + 80, g_font_small, status_color);

    // Cached credentials hint
    if (spotify_has_cached_credentials()) {
        render_text_centered("(Cached login found - auto-connecting)", g_screen_height / 2 + 120, g_font_small, COLOR_DIM);
    }

    // Footer
    render_text(BTN_B ": Back", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_spotify_search(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Check if searching - show loading indicator
    if (spsearch_get_state() == SPSEARCH_SEARCHING) {
        render_text_centered("Spotify Search", 20, g_font_medium, COLOR_ACCENT);
        render_text_centered("Searching...", g_screen_height / 2 - 80, g_font_large, COLOR_ACCENT);

        const char *query = spsearch_get_query();
        if (query && query[0]) {
            char display[128];
            snprintf(display, sizeof(display), "\"%s\"", query);
            render_text_centered(display, g_screen_height / 2 - 20, g_font_medium, COLOR_TEXT);
        }

        int monkey_x = (g_screen_width - 16 * MONKEY_PIXEL_SIZE) / 2;
        int monkey_y = g_screen_height / 2 + 40;
        render_monkey(monkey_x, monkey_y, true);

        render_text_centered("Please wait...", g_screen_height - 100, g_font_small, COLOR_DIM);

        render_version_watermark();
        SDL_RenderPresent(g_renderer);
        return;
    }

    // Title
    render_text_centered("Spotify Search", 20, g_font_medium, COLOR_ACCENT);

    // Current query with cursor
    const char *query = spsearch_get_query();
    int cursor_pos = spsearch_get_cursor();
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

    // Grid keyboard (QWERTY layout - same as YouTube search)
    int kbd_cols, kbd_rows;
    spsearch_get_kbd_size(&kbd_cols, &kbd_rows);
    int cur_row, cur_col;
    spsearch_get_kbd_pos(&cur_row, &cur_col);

    int kbd_y = 150;
    int cell_w = 100;
    int cell_h = 70;
    int kbd_w = kbd_cols * cell_w;
    int kbd_x = (g_screen_width - kbd_w) / 2;

    for (int row = 0; row < kbd_rows; row++) {
        for (int col = 0; col < kbd_cols; col++) {
            char c = spsearch_get_char_at(row, col);
            if (c == '\0') continue;

            int x = kbd_x + col * cell_w;
            int y = kbd_y + row * cell_h;

            bool is_selected = (row == cur_row && col == cur_col);

            if (is_selected) {
                draw_rect(x + 2, y + 2, cell_w - 4, cell_h - 4, COLOR_ACCENT);
            }

            char ch_str[2] = {c, '\0'};
            if (c == ' ') {
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

    // Error message
    const char *error = spsearch_get_error();
    if (error) {
        render_text_centered(error, kbd_y + kbd_rows * cell_h + 10, g_font_small, COLOR_ERROR);
    }

    // Controls
    int ctrl_y = g_screen_height - 100;
    render_text_centered("D-Pad: Move   " BTN_A ": Insert   " BTN_B ": Delete", ctrl_y, g_font_small, COLOR_DIM);
    render_text_centered(BTN_START ": Search   " BTN_SELECT ": Cancel", ctrl_y + 30, g_font_small, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

// Scroll state for Spotify results title
static int g_sp_scroll_offset = 0;
static int g_sp_scroll_cursor = -1;
static Uint32 g_sp_scroll_last_update = 0;
#define SP_SCROLL_SPEED_MS 100
#define SP_SCROLL_PAUSE_MS 1500

void ui_render_spotify_results(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    const char *query = spsearch_get_query();
    char header[128];
    snprintf(header, sizeof(header), "Spotify: %s", query);
    render_text(header, MARGIN, 8, g_font_small, COLOR_TEXT);

    // Results list
    int result_count = spsearch_get_result_count();
    int cursor = spsearch_get_results_cursor();
    int scroll = spsearch_get_scroll_offset();

    // Reset scroll when cursor changes
    if (cursor != g_sp_scroll_cursor) {
        g_sp_scroll_cursor = cursor;
        g_sp_scroll_offset = 0;
        g_sp_scroll_last_update = SDL_GetTicks() + SP_SCROLL_PAUSE_MS;
    }

    int y = HEADER_HEIGHT + 10;
    int visible = 8;
    int max_title_chars = 55;

    for (int i = 0; i < visible && (scroll + i) < result_count; i++) {
        int idx = scroll + i;
        const SpotifyTrack *track = spsearch_get_result(idx);
        if (!track) continue;

        bool is_selected = (idx == cursor);

        // Highlight selected item
        if (is_selected) {
            draw_rect(0, y - 5, g_screen_width, LINE_HEIGHT + 10, COLOR_HIGHLIGHT);
        }

        // Format duration
        char duration_str[16];
        spotify_format_duration(track->duration_ms, duration_str);

        // Title with scroll for selected long titles
        char title_display[256];
        int title_len = strlen(track->title);

        if (is_selected && title_len > max_title_chars) {
            Uint32 now = SDL_GetTicks();
            if (now > g_sp_scroll_last_update) {
                if (now - g_sp_scroll_last_update > SP_SCROLL_SPEED_MS) {
                    g_sp_scroll_offset++;
                    g_sp_scroll_last_update = now;
                    if (g_sp_scroll_offset > title_len - max_title_chars + 10) {
                        g_sp_scroll_offset = 0;
                        g_sp_scroll_last_update = now + SP_SCROLL_PAUSE_MS;
                    }
                }
            }

            int start = g_sp_scroll_offset;
            if (start > title_len) start = 0;
            snprintf(title_display, sizeof(title_display), "%.*s",
                     max_title_chars, track->title + start);
        } else if (title_len > max_title_chars) {
            snprintf(title_display, sizeof(title_display), "%.*s...",
                     max_title_chars - 3, track->title);
        } else {
            strncpy(title_display, track->title, sizeof(title_display) - 1);
            title_display[sizeof(title_display) - 1] = '\0';
        }

        // Render title
        render_text(title_display, MARGIN + 10, y,
                   g_font_small, is_selected ? COLOR_ACCENT : COLOR_TEXT);

        // Render artist, album, and duration
        char meta_display[256];
        snprintf(meta_display, sizeof(meta_display), "%.25s - %.20s  [%s]",
                 track->artist, track->album, duration_str);
        render_text(meta_display, MARGIN + 20, y + 28,
                   g_font_small, COLOR_DIM);

        y += LINE_HEIGHT + 10;
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

    // Controls
    render_text(BTN_A ": Play   " BTN_B ": Back to search", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);

    // Render toast overlay
    render_toast_overlay();

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_spotify_player(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Spotify", COLOR_TEXT, true);
    render_status_bar();
    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    // Get current track info
    const SpotifyTrack *track = spotify_get_current_track();

    // Center area for track info
    int center_y = g_screen_height / 2 - 60;

    if (track) {
        // Title
        render_text_centered(track->title, center_y, g_font_large, COLOR_TEXT);
        // Artist
        render_text_centered(track->artist, center_y + 70, g_font_medium, COLOR_DIM);
        // Album
        render_text_centered(track->album, center_y + 120, g_font_small, COLOR_DIM);
    } else {
        render_text_centered("Streaming via Spotify Connect", center_y, g_font_medium, COLOR_TEXT);
        render_text_centered("Control from your phone", center_y + 60, g_font_small, COLOR_DIM);
    }

    // Buffer status
    int buffered = sp_audio_buffered_seconds();
    bool receiving = sp_audio_is_receiving();

    char buf_status[64];
    snprintf(buf_status, sizeof(buf_status), "Buffer: %ds %s",
             buffered, receiving ? "[streaming]" : "[waiting]");
    render_text_centered(buf_status, g_screen_height - 120, g_font_small,
                        receiving ? COLOR_ACCENT : COLOR_DIM);

    // Dancing monkey (animates while streaming)
    int monkey_x = (g_screen_width - 16 * MONKEY_PIXEL_SIZE) / 2;
    int monkey_y = g_screen_height - 180;
    render_monkey(monkey_x, monkey_y, receiving);

    // Footer
    render_text(BTN_A ": Pause   " BTN_B ": Stop   Up/Down: Volume", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}

void ui_render_update(void) {
    // Clear screen
    SDL_SetRenderDrawColor(g_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(g_renderer);

    // Header
    render_header("> Update", COLOR_TEXT, false);
    render_status_bar();
    draw_rect(0, HEADER_HEIGHT, g_screen_width, 2, COLOR_DIM);

    UpdateState state = update_get_state();
    const UpdateInfo *info = update_get_info();

    int center_y = g_screen_height / 2;

    switch (state) {
        case UPDATE_IDLE:
        case UPDATE_CHECKING: {
            // Show checking animation with dancing monkey
            render_text_centered("Checking for Updates...", center_y - 80, g_font_large, COLOR_ACCENT);
            render_text_centered("Connecting to GitHub", center_y - 20, g_font_medium, COLOR_DIM);

            // Dancing monkey while checking
            int monkey_x = (g_screen_width - 16 * MONKEY_PIXEL_SIZE) / 2;
            int monkey_y = center_y + 40;
            render_monkey(monkey_x, monkey_y, true);
            break;
        }

        case UPDATE_AVAILABLE: {
            // Show version info and changelog
            render_text_centered("Update Available!", center_y - 140, g_font_large, COLOR_ACCENT);

            // Version comparison
            char version_text[128];
            snprintf(version_text, sizeof(version_text), "Current: v%s  ->  New: %s", VERSION, info->version);
            render_text_centered(version_text, center_y - 80, g_font_medium, COLOR_TEXT);

            // Changelog preview (first 3 lines)
            if (info->changelog[0]) {
                char changelog_preview[512];
                strncpy(changelog_preview, info->changelog, sizeof(changelog_preview) - 1);
                changelog_preview[sizeof(changelog_preview) - 1] = '\0';

                // Find first 3 lines
                char *line = strtok(changelog_preview, "\n");
                int line_y = center_y - 20;
                int lines = 0;
                while (line && lines < 4) {
                    if (line[0]) {  // Skip empty lines
                        render_text_centered(line, line_y, g_font_small, COLOR_DIM);
                        line_y += 30;
                        lines++;
                    }
                    line = strtok(NULL, "\n");
                }
            }

            // Size info
            if (info->size_bytes > 0) {
                char size_text[64];
                if (info->size_bytes > 1024 * 1024) {
                    snprintf(size_text, sizeof(size_text), "Size: %.1f MB",
                             (float)info->size_bytes / (1024 * 1024));
                } else {
                    snprintf(size_text, sizeof(size_text), "Size: %.0f KB",
                             (float)info->size_bytes / 1024);
                }
                render_text_centered(size_text, center_y + 100, g_font_small, COLOR_DIM);
            }

            // Footer
            render_text(BTN_A ": Download   " BTN_B ": Later", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
            break;
        }

        case UPDATE_DOWNLOADING: {
            // Show download progress
            render_text_centered("Downloading...", center_y - 80, g_font_large, COLOR_ACCENT);

            // Progress bar
            int bar_w = 600;
            int bar_h = 40;
            int bar_x = (g_screen_width - bar_w) / 2;
            int bar_y = center_y - 10;

            // Background
            draw_rect(bar_x, bar_y, bar_w, bar_h, COLOR_DIM);

            // Progress fill
            int progress = update_get_progress();
            int fill_w = (bar_w - 4) * progress / 100;
            draw_rect(bar_x + 2, bar_y + 2, fill_w, bar_h - 4, COLOR_ACCENT);

            // Progress text
            char progress_text[32];
            snprintf(progress_text, sizeof(progress_text), "%d%%", progress);
            render_text_centered(progress_text, bar_y + bar_h + 30, g_font_medium, COLOR_TEXT);

            // Dancing monkey
            int monkey_x = (g_screen_width - 16 * MONKEY_PIXEL_SIZE) / 2;
            int monkey_y = center_y + 100;
            render_monkey(monkey_x, monkey_y, true);
            break;
        }

        case UPDATE_READY: {
            // Update applied successfully
            render_text_centered("Update Ready!", center_y - 60, g_font_large, COLOR_ACCENT);
            render_text_centered("Restart the app to use the new version.", center_y + 10, g_font_medium, COLOR_TEXT);

            char version_text[64];
            snprintf(version_text, sizeof(version_text), "Updated to %s", info->version);
            render_text_centered(version_text, center_y + 70, g_font_medium, COLOR_DIM);

            // Footer
            render_text(BTN_B ": OK", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
            break;
        }

        case UPDATE_UP_TO_DATE: {
            // Already on latest version
            render_text_centered("You're up to date!", center_y - 40, g_font_large, COLOR_ACCENT);

            char version_text[64];
            snprintf(version_text, sizeof(version_text), "Version: v%s", VERSION);
            render_text_centered(version_text, center_y + 30, g_font_medium, COLOR_DIM);

            // Footer
            render_text(BTN_B ": OK", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
            break;
        }

        case UPDATE_ERROR: {
            // Error state
            const char *error = update_get_error();
            render_text_centered("Update Failed", center_y - 60, g_font_large, COLOR_ERROR);

            if (error) {
                render_text_centered(error, center_y + 10, g_font_medium, COLOR_DIM);
            }

            // Footer
            render_text(BTN_A ": Retry   " BTN_B ": Cancel", MARGIN, g_screen_height - SCREEN_PAD - 22, g_font_hint, COLOR_DIM);
            break;
        }
    }

    render_version_watermark();
    SDL_RenderPresent(g_renderer);
}
