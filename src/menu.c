/**
 * Menu System Implementation
 *
 * Context-sensitive menu with mode-based item arrays:
 * - Player mode:  Shuffle, Repeat, Sleep, Equalizer
 * - Browser mode: Theme, Power
 */

#include "menu.h"
#include "theme.h"
#include "state.h"
#include "equalizer.h"
#include <SDL2/SDL.h>
#include <stdio.h>

// Menu state
static MenuMode g_mode = MENU_MODE_BROWSER;
static int g_cursor = 0;
static bool g_shuffle = false;
static RepeatMode g_repeat = REPEAT_OFF;
static PowerMode g_power_mode = POWER_MODE_BALANCED;
static int g_sleep_minutes = 0;  // 0, 15, 30, 60
static Uint32 g_sleep_end_ticks = 0;

// Sleep timer options (in minutes)
static const int SLEEP_OPTIONS[] = {0, 15, 30, 60};
static const int SLEEP_OPTIONS_COUNT = 4;
static int g_sleep_option_index = 0;

// Item arrays per mode
static const MenuItem PLAYER_ITEMS[] = { MENU_SHUFFLE, MENU_REPEAT, MENU_SLEEP, MENU_EQUALIZER };
static const int PLAYER_ITEM_COUNT = 4;

static const MenuItem BROWSER_ITEMS[] = { MENU_THEME, MENU_POWER, MENU_UPDATE };
static const int BROWSER_ITEM_COUNT = 3;

// Label buffer for dynamic labels
static char g_label_buf[64];

/**
 * Get the active items array and count for current mode
 */
static const MenuItem* get_active_items(void) {
    return (g_mode == MENU_MODE_PLAYER) ? PLAYER_ITEMS : BROWSER_ITEMS;
}

static int get_active_count(void) {
    return (g_mode == MENU_MODE_PLAYER) ? PLAYER_ITEM_COUNT : BROWSER_ITEM_COUNT;
}

void menu_init(void) {
    g_mode = MENU_MODE_BROWSER;
    g_cursor = 0;
    g_shuffle = false;
    g_repeat = REPEAT_OFF;
    g_power_mode = POWER_MODE_BALANCED;
    g_sleep_minutes = 0;
    g_sleep_end_ticks = 0;
    g_sleep_option_index = 0;
}

void menu_open(MenuMode mode) {
    g_mode = mode;
    g_cursor = 0;
}

void menu_move_cursor(int direction) {
    int count = get_active_count();
    g_cursor += direction;
    if (g_cursor < 0) g_cursor = count - 1;
    if (g_cursor >= count) g_cursor = 0;
}

MenuResult menu_select(void) {
    MenuItem item = menu_get_current_item();

    switch (item) {
        case MENU_SHUFFLE:
            g_shuffle = !g_shuffle;
            printf("[MENU] Shuffle: %s\n", g_shuffle ? "ON" : "OFF");
            return MENU_RESULT_NONE;

        case MENU_REPEAT:
            g_repeat = (g_repeat + 1) % 3;
            printf("[MENU] Repeat: %s\n", menu_get_repeat_string());
            return MENU_RESULT_NONE;

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
            return MENU_RESULT_NONE;

        case MENU_EQUALIZER:
            printf("[MENU] Equalizer selected\n");
            return MENU_RESULT_EQUALIZER;

        case MENU_THEME:
            theme_cycle();
            return MENU_RESULT_NONE;

        case MENU_POWER:
            g_power_mode = (g_power_mode + 1) % 3;
            printf("[MENU] Power mode: %s\n", menu_get_power_string());
            state_notify_settings_changed();
            return MENU_RESULT_NONE;

        case MENU_UPDATE:
            printf("[MENU] Check for Updates selected\n");
            return MENU_RESULT_UPDATE;

        default:
            return MENU_RESULT_NONE;
    }
}

int menu_get_cursor(void) {
    return g_cursor;
}

int menu_get_item_count(void) {
    return get_active_count();
}

const char* menu_get_item_label(int index) {
    int count = get_active_count();
    if (index < 0 || index >= count) return "";

    const MenuItem *items = get_active_items();
    MenuItem item = items[index];

    switch (item) {
        case MENU_SHUFFLE:
            snprintf(g_label_buf, sizeof(g_label_buf), "Shuffle: %s",
                     g_shuffle ? "On" : "Off");
            break;
        case MENU_REPEAT:
            snprintf(g_label_buf, sizeof(g_label_buf), "Repeat: %s",
                     menu_get_repeat_string());
            break;
        case MENU_SLEEP:
            snprintf(g_label_buf, sizeof(g_label_buf), "Sleep: %s",
                     menu_get_sleep_string());
            break;
        case MENU_EQUALIZER:
            snprintf(g_label_buf, sizeof(g_label_buf), "Equalizer");
            break;
        case MENU_THEME:
            snprintf(g_label_buf, sizeof(g_label_buf), "Theme: %s",
                     theme_get_current_name());
            break;
        case MENU_POWER:
            snprintf(g_label_buf, sizeof(g_label_buf), "Power: %s",
                     menu_get_power_string());
            break;
        case MENU_UPDATE:
            snprintf(g_label_buf, sizeof(g_label_buf), "Check for Updates");
            break;
        default:
            g_label_buf[0] = '\0';
            break;
    }
    return g_label_buf;
}

MenuItem menu_get_current_item(void) {
    const MenuItem *items = get_active_items();
    int count = get_active_count();
    if (g_cursor >= 0 && g_cursor < count) {
        return items[g_cursor];
    }
    return items[0];
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

void menu_set_shuffle(bool enabled) {
    g_shuffle = enabled;
    printf("[MENU] Shuffle set to: %s\n", g_shuffle ? "ON" : "OFF");
}

void menu_set_repeat(RepeatMode mode) {
    g_repeat = mode;
    printf("[MENU] Repeat set to: %s\n", menu_get_repeat_string());
}

PowerMode menu_get_power_mode(void) {
    return g_power_mode;
}

void menu_set_power_mode(PowerMode mode) {
    g_power_mode = mode;
    printf("[MENU] Power mode set to: %s\n", menu_get_power_string());
}

const char* menu_get_power_string(void) {
    switch (g_power_mode) {
        case POWER_MODE_BATTERY:     return "Battery";
        case POWER_MODE_BALANCED:    return "Balanced";
        case POWER_MODE_PERFORMANCE: return "Performance";
        default: return "Balanced";
    }
}
