/**
 * Cover Art Implementation
 *
 * Uses stb_image for image loading (single-header, no dependencies).
 * Supports PNG, JPG, JPEG formats.
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

#include "cover.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Cover display size (scaled to fit)
#define COVER_MAX_SIZE 150

// Cached cover texture
static SDL_Texture *g_cover_texture = NULL;
static SDL_Renderer *g_renderer = NULL;
static int g_cover_width = 0;
static int g_cover_height = 0;
static char g_current_dir[512] = {0};
static bool g_cover_is_dark = true;  // Default to dark (for safety with light text)

// Cover filename patterns to search (in priority order)
// Reduced list - most common names only, case handled in search
static const char *COVER_BASENAMES[] = {"cover", "folder", "album", "front", NULL};
static const char *COVER_EXTENSIONS[] = {".jpg", ".png", ".jpeg", NULL};

/**
 * Check if file exists
 */
static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/**
 * Analyze image brightness by sampling pixels
 * Returns true if image is predominantly dark
 */
static bool analyze_brightness(unsigned char *data, int width, int height) {
    // Sample every Nth pixel for performance (don't need to check all)
    int sample_step = (width * height > 10000) ? 10 : 1;
    long total_luminance = 0;
    int sample_count = 0;

    for (int y = 0; y < height; y += sample_step) {
        for (int x = 0; x < width; x += sample_step) {
            int idx = (y * width + x) * 4;  // RGBA format
            unsigned char r = data[idx];
            unsigned char g = data[idx + 1];
            unsigned char b = data[idx + 2];

            // Calculate perceived luminance (weighted for human perception)
            // Formula: 0.299*R + 0.587*G + 0.114*B
            int luminance = (299 * r + 587 * g + 114 * b) / 1000;
            total_luminance += luminance;
            sample_count++;
        }
    }

    if (sample_count == 0) return true;  // Default to dark

    int avg_luminance = total_luminance / sample_count;

    // Threshold: below 128 is considered dark
    // Using 100 as threshold to account for overlay darkening
    bool is_dark = avg_luminance < 100;

    printf("[COVER] Brightness analysis: avg=%d, is_dark=%s\n",
           avg_luminance, is_dark ? "yes" : "no");

    return is_dark;
}

/**
 * Load image and create SDL texture
 */
static SDL_Texture* load_image(const char *path) {
    int width, height, channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 4); // Force RGBA

    if (!data) {
        fprintf(stderr, "[COVER] Failed to load image: %s\n", stbi_failure_reason());
        return NULL;
    }

    // Analyze brightness before converting to texture
    g_cover_is_dark = analyze_brightness(data, width, height);

    // Create SDL surface from raw pixel data
    // Use SDL_PIXELFORMAT_ABGR8888 which is common on ARM/Mali GPUs
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        data, width, height, 32, width * 4, SDL_PIXELFORMAT_ABGR8888
    );

    if (!surface) {
        fprintf(stderr, "[COVER] Failed to create surface: %s\n", SDL_GetError());
        stbi_image_free(data);
        return NULL;
    }

    // Create texture from surface
    SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, surface);

    SDL_FreeSurface(surface);
    stbi_image_free(data);

    if (!texture) {
        fprintf(stderr, "[COVER] Failed to create texture: %s\n", SDL_GetError());
        return NULL;
    }

    // Calculate scaled dimensions (maintain aspect ratio)
    float scale = 1.0f;
    if (width > COVER_MAX_SIZE || height > COVER_MAX_SIZE) {
        float scale_x = (float)COVER_MAX_SIZE / width;
        float scale_y = (float)COVER_MAX_SIZE / height;
        scale = (scale_x < scale_y) ? scale_x : scale_y;
    }

    g_cover_width = (int)(width * scale);
    g_cover_height = (int)(height * scale);

    printf("[COVER] Loaded %s (%dx%d -> %dx%d)\n", path, width, height, g_cover_width, g_cover_height);

    return texture;
}

int cover_init(SDL_Renderer *renderer) {
    g_renderer = renderer;
    g_cover_texture = NULL;
    g_cover_width = 0;
    g_cover_height = 0;
    g_current_dir[0] = '\0';
    return 0;
}

void cover_cleanup(void) {
    cover_clear();
    g_renderer = NULL;
}

bool cover_load(const char *dir_path) {
    if (!dir_path || !g_renderer) {
        return false;
    }

    // Skip if same directory (already cached)
    if (g_current_dir[0] && strcmp(g_current_dir, dir_path) == 0 && g_cover_texture) {
        return true;  // Already loaded
    }

    // Clear previous cover
    cover_clear();

    // Store current directory
    strncpy(g_current_dir, dir_path, sizeof(g_current_dir) - 1);
    g_current_dir[sizeof(g_current_dir) - 1] = '\0';

    // Search for cover files (optimized: basename x extension matrix)
    char path[512];
    char upper_name[32];

    for (int b = 0; COVER_BASENAMES[b] != NULL; b++) {
        for (int e = 0; COVER_EXTENSIONS[e] != NULL; e++) {
            // Try lowercase
            snprintf(path, sizeof(path), "%s/%s%s", dir_path, COVER_BASENAMES[b], COVER_EXTENSIONS[e]);
            if (file_exists(path)) {
                g_cover_texture = load_image(path);
                if (g_cover_texture) return true;
            }

            // Try capitalized (Cover.jpg, Folder.png, etc.)
            snprintf(upper_name, sizeof(upper_name), "%c%s", COVER_BASENAMES[b][0] - 32, COVER_BASENAMES[b] + 1);
            snprintf(path, sizeof(path), "%s/%s%s", dir_path, upper_name, COVER_EXTENSIONS[e]);
            if (file_exists(path)) {
                g_cover_texture = load_image(path);
                if (g_cover_texture) return true;
            }
        }
    }

    printf("[COVER] No cover found in %s\n", dir_path);
    return false;
}

SDL_Texture* cover_get_texture(void) {
    return g_cover_texture;
}

void cover_get_size(int *w, int *h) {
    if (w) *w = g_cover_width;
    if (h) *h = g_cover_height;
}

bool cover_is_loaded(void) {
    return g_cover_texture != NULL;
}

void cover_clear(void) {
    if (g_cover_texture) {
        SDL_DestroyTexture(g_cover_texture);
        g_cover_texture = NULL;
    }
    g_cover_width = 0;
    g_cover_height = 0;
    g_current_dir[0] = '\0';
    g_cover_is_dark = true;  // Reset to default
}

bool cover_is_dark(void) {
    return g_cover_is_dark;
}
