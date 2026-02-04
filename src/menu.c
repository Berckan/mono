/**
 * Menu System Implementation
 */

#include "menu.h"
#include <SDL2/SDL.h>
#include <stdio.h>

// Menu state
static int g_cursor = 0;
static bool g_shuffle = false;
static RepeatMode g_repeat = REPEAT_OFF;
static int g_sleep_minutes = 0;  // 0, 15, 30, 60
static Uint32 g_sleep_end_ticks = 0;

// Sleep timer options (in minutes)
static const int SLEEP_OPTIONS[] = {0, 15, 30, 60};
static const int SLEEP_OPTIONS_COUNT = 4;
static int g_sleep_option_index = 0;

void menu_init(void) {
    g_cursor = 0;
    g_shuffle = false;
    g_repeat = REPEAT_OFF;
    g_sleep_minutes = 0;
    g_sleep_end_ticks = 0;
    g_sleep_option_index = 0;
}

void menu_move_cursor(int direction) {
    g_cursor += direction;
    if (g_cursor < 0) g_cursor = MENU_ITEM_COUNT - 1;
    if (g_cursor >= MENU_ITEM_COUNT) g_cursor = 0;
}

bool menu_select(void) {
    switch (g_cursor) {
        case MENU_SHUFFLE:
            g_shuffle = !g_shuffle;
            printf("[MENU] Shuffle: %s\n", g_shuffle ? "ON" : "OFF");
            break;

        case MENU_REPEAT:
            g_repeat = (g_repeat + 1) % 3;
            printf("[MENU] Repeat: %s\n", menu_get_repeat_string());
            break;

        case MENU_SLEEP:
            g_sleep_option_index = (g_sleep_option_index + 1) % SLEEP_OPTIONS_COUNT;
            g_sleep_minutes = SLEEP_OPTIONS[g_sleep_option_index];
            if (g_sleep_minutes > 0) {
                g_sleep_end_ticks = SDL_GetTicks() + (g_sleep_minutes * 60 * 1000);
                printf("[MENU] Sleep timer: %d min\n", g_sleep_minutes);
            } else {
                g_sleep_end_ticks = 0;
                printf("[MENU] Sleep timer: OFF\n");
            }
            break;

        case MENU_EXIT:
            printf("[MENU] Exit selected\n");
            return true;

        default:
            break;
    }
    return false;
}

int menu_get_cursor(void) {
    return g_cursor;
}

bool menu_is_shuffle_enabled(void) {
    return g_shuffle;
}

RepeatMode menu_get_repeat_mode(void) {
    return g_repeat;
}

int menu_get_sleep_remaining(void) {
    if (g_sleep_end_ticks == 0) return 0;

    Uint32 now = SDL_GetTicks();
    if (now >= g_sleep_end_ticks) return 0;

    return (g_sleep_end_ticks - now) / 60000 + 1;  // Round up to minutes
}

bool menu_update_sleep_timer(void) {
    if (g_sleep_end_ticks == 0) return false;

    if (SDL_GetTicks() >= g_sleep_end_ticks) {
        printf("[MENU] Sleep timer expired!\n");
        g_sleep_end_ticks = 0;
        g_sleep_minutes = 0;
        g_sleep_option_index = 0;
        return true;
    }
    return false;
}

const char* menu_get_repeat_string(void) {
    switch (g_repeat) {
        case REPEAT_OFF: return "Off";
        case REPEAT_ONE: return "One";
        case REPEAT_ALL: return "All";
        default: return "Off";
    }
}

const char* menu_get_sleep_string(void) {
    static char buf[16];
    if (g_sleep_end_ticks == 0) {
        return "Off";
    }
    int remaining = menu_get_sleep_remaining();
    snprintf(buf, sizeof(buf), "%d min", remaining);
    return buf;
}
