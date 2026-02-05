/**
 * Theme System Implementation
 */

#include "theme.h"
#include <stdio.h>

// Theme definitions
static const ThemeColors THEMES[THEME_COUNT] = {
    // THEME_DARK - Original dark theme
    {
        .bg        = {18, 18, 18, 255},      // Near black (original)
        .text      = {255, 255, 255, 255},   // White
        .dim       = {140, 140, 170, 255},   // Light Blue-Gray
        .accent    = {51, 255, 51, 255},     // Neon Green
        .highlight = {64, 64, 96, 255},      // Selection Blue
        .error     = {255, 51, 51, 255}      // Retro Red
    },
    // THEME_LIGHT - Clean light theme
    {
        .bg        = {240, 240, 245, 255},   // Light Gray
        .text      = {30, 30, 40, 255},      // Dark Gray
        .dim       = {120, 120, 140, 255},   // Medium Gray
        .accent    = {0, 150, 80, 255},      // Forest Green
        .highlight = {200, 210, 220, 255},   // Light Blue-Gray
        .error     = {200, 50, 50, 255}      // Dark Red
    }
};

static const char* THEME_NAMES[THEME_COUNT] = {
    "Dark",
    "Light"
};

// Current theme
static ThemeId g_current_theme = THEME_DARK;

void theme_init(void) {
    g_current_theme = THEME_DARK;
    printf("[THEME] Initialized with Dark theme\n");
}

ThemeId theme_get_current(void) {
    return g_current_theme;
}

void theme_set(ThemeId id) {
    if (id >= 0 && id < THEME_COUNT) {
        g_current_theme = id;
        printf("[THEME] Set to: %s\n", THEME_NAMES[id]);
    }
}

void theme_cycle(void) {
    g_current_theme = (g_current_theme + 1) % THEME_COUNT;
    printf("[THEME] Cycled to: %s\n", THEME_NAMES[g_current_theme]);
}

const ThemeColors* theme_get_colors(void) {
    return &THEMES[g_current_theme];
}

const char* theme_get_name(ThemeId id) {
    if (id >= 0 && id < THEME_COUNT) {
        return THEME_NAMES[id];
    }
    return "Unknown";
}

const char* theme_get_current_name(void) {
    return THEME_NAMES[g_current_theme];
}
