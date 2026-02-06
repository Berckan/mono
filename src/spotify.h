/**
 * Spotify Integration via librespot + Web API
 *
 * Provides Spotify streaming using librespot for audio playback
 * and Spotify Web API (via curl) for search/browse.
 *
 * Auth: Spotify Connect (Zeroconf/mDNS) - phone discovers device.
 * Audio: librespot → named FIFO pipe → PCM reader → SDL_mixer.
 * Search: curl → Spotify Web API → cJSON parsing.
 */

#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <stdbool.h>

// Maximum search results
#define SPOTIFY_MAX_RESULTS 10

// Spotify connection/auth states
typedef enum {
    SP_STATE_DISCONNECTED,  // Not started
    SP_STATE_WAITING,       // librespot running, waiting for phone connect
    SP_STATE_CONNECTED,     // Phone connected, auth token received
    SP_STATE_PLAYING,       // Audio streaming
    SP_STATE_PAUSED,        // Paused by user
    SP_STATE_ERROR          // Error state
} SpotifyState;

// Track info from Spotify Web API search
typedef struct {
    char uri[64];           // spotify:track:xxxx
    char title[256];
    char artist[128];
    char album[128];
    int duration_ms;
} SpotifyTrack;

/**
 * Initialize Spotify system
 * Checks for librespot and curl availability
 */
void spotify_init(void);

/**
 * Cleanup Spotify resources
 * Kills librespot process, removes FIFO pipe
 */
void spotify_cleanup(void);

/**
 * Check if Spotify functionality is available
 * @return true if librespot and curl are found
 */
bool spotify_is_available(void);

/**
 * Start librespot daemon with Spotify Connect discovery
 * Creates FIFO pipe, starts librespot process
 * @return true if librespot started successfully
 */
bool spotify_start_connect(void);

/**
 * Stop librespot daemon
 */
void spotify_stop_connect(void);

/**
 * Get current connection/auth state
 */
SpotifyState spotify_get_state(void);

/**
 * Check if librespot has received auth from phone
 * (Polls librespot event pipe for connection events)
 * @return true if a device connected
 */
bool spotify_check_connected(void);

// ============================================================================
// Web API Search (requires client_id/client_secret)
// ============================================================================

/**
 * Authenticate with Spotify Web API (Client Credentials flow)
 * Uses SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET from config
 * @return true if token obtained
 */
bool spotify_api_authenticate(void);

/**
 * Search Spotify catalog
 * @param query Search query string
 * @param results Array to fill with results
 * @param max_results Maximum results (up to SPOTIFY_MAX_RESULTS)
 * @return Number of results found, or -1 on error
 */
int spotify_search(const char *query, SpotifyTrack *results, int max_results);

/**
 * Play a Spotify track via librespot
 * Sends play command to librespot via its control interface
 * @param uri Spotify URI (e.g., "spotify:track:xxxxx")
 * @return true if play command sent
 */
bool spotify_play_track(const char *uri);

/**
 * Pause/resume playback
 */
void spotify_toggle_pause(void);

/**
 * Stop playback
 */
void spotify_stop_playback(void);

/**
 * Check if audio is actively streaming from pipe
 * @return true if PCM data is flowing
 */
bool spotify_is_streaming(void);

/**
 * Get the currently playing track info
 * @return Pointer to track info, or NULL
 */
const SpotifyTrack* spotify_get_current_track(void);

/**
 * Get playback position in milliseconds
 */
int spotify_get_position_ms(void);

/**
 * Get last error message
 * @return Error message string, or NULL
 */
const char* spotify_get_error(void);

/**
 * Format duration in ms to MM:SS string
 * @param duration_ms Duration in milliseconds
 * @param buffer Output buffer (at least 16 chars)
 */
void spotify_format_duration(int duration_ms, char *buffer);

/**
 * Get the Spotify cache directory path
 * @return Path to cache directory
 */
const char* spotify_get_cache_dir(void);

/**
 * Check if we have cached credentials (previous auth)
 * @return true if cached credentials exist
 */
bool spotify_has_cached_credentials(void);

#endif // SPOTIFY_H
