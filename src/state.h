/**
 * State Persistence - Save/restore application state
 *
 * Handles persisting playback state (resume functionality) and user
 * preferences to JSON files in the user data directory.
 *
 * Path: ~/.userdata/tg5040/Mono/state.json
 */

#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
#include "menu.h"
#include "theme.h"

/**
 * Persisted application state
 */
typedef struct {
    // Last played track
    char last_file[512];          // Full path to last played file
    char last_folder[512];        // Directory containing last file
    int last_position;            // Playback position in seconds
    int last_cursor;              // Cursor position in browser

    // User preferences
    int volume;                   // Volume level (0-100)
    bool shuffle;                 // Shuffle mode enabled
    RepeatMode repeat;            // Repeat mode (OFF/ONE/ALL)
    ThemeId theme;                // UI theme (DARK/LIGHT)
    PowerMode power_mode;         // Power mode (BATTERY/BALANCED/PERFORMANCE)
    int eq_bands[5];              // Equalizer bands 0-4 (-12 to +12 dB)

    // State flags
    bool was_playing;             // Was playback active when app closed
    bool has_resume_data;         // Is there valid data to resume from
} AppStateData;

/**
 * Initialize state persistence system
 * Creates data directory if it doesn't exist
 * @return 0 on success, -1 on failure
 */
int state_init(void);

/**
 * Cleanup state resources
 */
void state_cleanup(void);

/**
 * Save current application state
 * Called before app exit
 * @param data Pointer to state data to save
 * @return true if saved successfully
 */
bool state_save(const AppStateData *data);

/**
 * Load previously saved state
 * @param data Pointer to state struct to populate
 * @return true if valid state was loaded
 */
bool state_load(AppStateData *data);

/**
 * Clear saved state (delete state file)
 * Call when user explicitly stops playback
 */
void state_clear(void);

/**
 * Get the data directory path
 * @return Path to Mono's data directory
 */
const char* state_get_data_dir(void);

/**
 * Settings changed callback type
 */
typedef void (*SettingsChangedCallback)(void);

/**
 * Register a callback to be called when settings change
 * Used by main.c to register save_app_state
 * @param callback Function to call when settings change
 */
void state_set_settings_callback(SettingsChangedCallback callback);

/**
 * Notify that settings have changed (triggers save)
 * Called by menu.c when user changes power mode, etc.
 */
void state_notify_settings_changed(void);

#endif // STATE_H
