/**
 * Spotify Integration Implementation
 *
 * Uses librespot for Spotify Connect auth + audio streaming.
 * Uses curl + Spotify Web API for search/browse.
 * Follows patterns from youtube.c for CLI execution and cJSON parsing.
 */

#include "spotify.h"
#include "state.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

// Paths for librespot binary (same order-of-preference as youtube.c)
#define LIBRESPOT_BUNDLED_REL "./bin/librespot"
#define LIBRESPOT_BUNDLED_ABS "Mono.pak/bin/librespot"
#define LIBRESPOT_SYSTEM      "librespot"

// Named FIFO pipe for PCM audio from librespot
#define SPOTIFY_FIFO_PATH "/tmp/mono_spotify"

// Temp file for API responses
#define TEMP_API_FILE "/tmp/mono_sp_api.json"

// Config file for API credentials
#define SPOTIFY_CONFIG_FILE "spotify.json"

// API token validity (1 hour, refresh at 55 min)
#define TOKEN_REFRESH_SEC 3300

// librespot event file (librespot writes events here)
#define LIBRESPOT_EVENT_FILE "/tmp/mono_sp_events"

// PID file written by shell when backgrounding librespot
#define LIBRESPOT_PID_FILE "/tmp/mono_librespot.pid"

// Script file for librespot --onevent (librespot uses Command::new, not shell)
#define LIBRESPOT_EVENT_SCRIPT "/tmp/mono_sp_event.sh"

// Current state
static bool g_available = false;
static char g_librespot_path[256] = {0};
static char g_cache_dir[256] = {0};
static char g_error[256] = {0};
static SpotifyState g_state = SP_STATE_DISCONNECTED;
static pid_t g_librespot_pid = 0;

// API credentials and token
static char g_client_id[128] = {0};
static char g_client_secret[128] = {0};
static char g_access_token[512] = {0};
static time_t g_token_expires = 0;

// Current track
static SpotifyTrack g_current_track;
static bool g_has_current_track = false;
static int g_position_ms = 0;

/**
 * Check if a file exists and is executable
 */
static bool file_executable(const char *path) {
    return access(path, X_OK) == 0;
}

/**
 * Check if a command exists on PATH
 */
static bool command_exists(const char *cmd) {
    char which_cmd[128];
    snprintf(which_cmd, sizeof(which_cmd), "which %s >/dev/null 2>&1", cmd);
    return system(which_cmd) == 0;
}

/**
 * Load API credentials from config file in data directory
 */
static bool load_api_credentials(void) {
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/%s", state_get_data_dir(), SPOTIFY_CONFIG_FILE);

    FILE *f = fopen(config_path, "r");
    if (!f) {
        printf("[SPOTIFY] No config file at %s\n", config_path);
        return false;
    }

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 4096) {
        fclose(f);
        return false;
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read = fread(buf, 1, size, f);
    fclose(f);
    buf[read] = '\0';

    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json) {
        printf("[SPOTIFY] Failed to parse config JSON\n");
        return false;
    }

    cJSON *id = cJSON_GetObjectItem(json, "client_id");
    cJSON *secret = cJSON_GetObjectItem(json, "client_secret");

    if (id && id->valuestring && secret && secret->valuestring) {
        strncpy(g_client_id, id->valuestring, sizeof(g_client_id) - 1);
        strncpy(g_client_secret, secret->valuestring, sizeof(g_client_secret) - 1);
        cJSON_Delete(json);
        printf("[SPOTIFY] API credentials loaded\n");
        return true;
    }

    cJSON_Delete(json);
    printf("[SPOTIFY] Config missing client_id or client_secret\n");
    return false;
}

/**
 * Kill librespot process if running
 */
static void kill_librespot(void) {
    if (g_librespot_pid > 0) {
        kill(g_librespot_pid, SIGTERM);
        // Give it a moment to clean up
        usleep(100000);  // 100ms
        // Force kill if still running
        kill(g_librespot_pid, SIGKILL);
        g_librespot_pid = 0;
        printf("[SPOTIFY] librespot killed\n");
    }
}

/**
 * Create FIFO pipe for audio output
 */
static bool create_fifo(void) {
    // Remove existing pipe
    unlink(SPOTIFY_FIFO_PATH);

    if (mkfifo(SPOTIFY_FIFO_PATH, 0666) != 0) {
        snprintf(g_error, sizeof(g_error), "Failed to create audio pipe: %s", strerror(errno));
        fprintf(stderr, "[SPOTIFY] mkfifo failed: %s\n", strerror(errno));
        return false;
    }

    printf("[SPOTIFY] Created FIFO pipe: %s\n", SPOTIFY_FIFO_PATH);
    return true;
}

void spotify_init(void) {
    g_available = false;
    g_librespot_path[0] = '\0';
    g_error[0] = '\0';
    g_state = SP_STATE_DISCONNECTED;
    g_librespot_pid = 0;
    g_has_current_track = false;
    g_position_ms = 0;

    // Set cache directory
    snprintf(g_cache_dir, sizeof(g_cache_dir), "%s/spotify_cache", state_get_data_dir());
    mkdir(g_cache_dir, 0755);

    // Check for librespot binary (bundled → system, same as youtube.c)
    if (file_executable(LIBRESPOT_BUNDLED_REL)) {
        strncpy(g_librespot_path, LIBRESPOT_BUNDLED_REL, sizeof(g_librespot_path) - 1);
        g_available = true;
        printf("[SPOTIFY] Using bundled librespot: %s\n", g_librespot_path);
    } else if (file_executable(LIBRESPOT_BUNDLED_ABS)) {
        strncpy(g_librespot_path, LIBRESPOT_BUNDLED_ABS, sizeof(g_librespot_path) - 1);
        g_available = true;
        printf("[SPOTIFY] Using bundled librespot: %s\n", g_librespot_path);
    } else if (command_exists(LIBRESPOT_SYSTEM)) {
        strncpy(g_librespot_path, LIBRESPOT_SYSTEM, sizeof(g_librespot_path) - 1);
        g_available = true;
        printf("[SPOTIFY] Using system librespot\n");
    } else {
        printf("[SPOTIFY] librespot not found - Spotify features disabled\n");
        snprintf(g_error, sizeof(g_error), "librespot not found");
        return;
    }

    // Also need curl for Web API
    if (!command_exists("curl")) {
        printf("[SPOTIFY] curl not found - Spotify search disabled\n");
        // Still available for Spotify Connect (just no search)
    }

    // Load API credentials
    load_api_credentials();

    printf("[SPOTIFY] Initialized (available=%d)\n", g_available);
}

void spotify_cleanup(void) {
    kill_librespot();
    unlink(SPOTIFY_FIFO_PATH);
    unlink(TEMP_API_FILE);
    unlink(LIBRESPOT_EVENT_FILE);
    unlink(LIBRESPOT_PID_FILE);
    unlink(LIBRESPOT_EVENT_SCRIPT);
    g_state = SP_STATE_DISCONNECTED;
    printf("[SPOTIFY] Cleanup complete\n");
}

bool spotify_is_available(void) {
    return g_available;
}

bool spotify_start_connect(void) {
    if (!g_available) {
        snprintf(g_error, sizeof(g_error), "librespot not available");
        return false;
    }

    // Kill any existing instance
    kill_librespot();

    // Create FIFO pipe
    if (!create_fifo()) {
        return false;
    }

    // Create event handler script for librespot --onevent.
    // librespot uses Command::new() (not shell), so $PLAYER_EVENT won't expand
    // in an inline command. A script file lets the shell interpret env vars.
    FILE *script = fopen(LIBRESPOT_EVENT_SCRIPT, "w");
    if (script) {
        fprintf(script, "#!/bin/sh\necho \"$PLAYER_EVENT\" >> %s\n", LIBRESPOT_EVENT_FILE);
        fclose(script);
        chmod(LIBRESPOT_EVENT_SCRIPT, 0755);
    }

    // Clear event and PID files
    unlink(LIBRESPOT_EVENT_FILE);
    unlink(LIBRESPOT_PID_FILE);

    // Launch librespot in background, capture PID via $! written to pid file.
    // BusyBox pgrep -f matches itself, so we use $! instead.
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "%s --name 'Mono' "
        "--backend pipe "
        "--device '%s' "
        "--cache '%s' "
        "--bitrate 160 "
        "--format S16 "
        "--initial-volume 100 "
        "--onevent %s "
        ">/dev/null 2>&1 & echo $! > %s",
        g_librespot_path,
        SPOTIFY_FIFO_PATH,
        g_cache_dir,
        LIBRESPOT_EVENT_SCRIPT,
        LIBRESPOT_PID_FILE);

    printf("[SPOTIFY] Starting librespot: %s\n", cmd);

    int ret = system(cmd);
    if (ret != 0) {
        snprintf(g_error, sizeof(g_error), "Failed to start librespot");
        fprintf(stderr, "[SPOTIFY] librespot start failed: %d\n", ret);
        return false;
    }

    // Read PID from file (written by shell's $!)
    FILE *pid_file = fopen(LIBRESPOT_PID_FILE, "r");
    if (pid_file) {
        char pid_buf[32];
        if (fgets(pid_buf, sizeof(pid_buf), pid_file)) {
            g_librespot_pid = atoi(pid_buf);
        }
        fclose(pid_file);
    }

    printf("[SPOTIFY] librespot started (pid=%d)\n", g_librespot_pid);
    g_state = SP_STATE_WAITING;
    g_error[0] = '\0';
    return true;
}

void spotify_stop_connect(void) {
    kill_librespot();
    g_state = SP_STATE_DISCONNECTED;
    g_has_current_track = false;
}

SpotifyState spotify_get_state(void) {
    return g_state;
}

bool spotify_check_connected(void) {
    if (g_state != SP_STATE_WAITING) return false;

    // Check if librespot process is still running
    if (g_librespot_pid > 0 && kill(g_librespot_pid, 0) != 0) {
        // Process died
        snprintf(g_error, sizeof(g_error), "librespot exited unexpectedly");
        g_state = SP_STATE_ERROR;
        g_librespot_pid = 0;
        return false;
    }

    // Check event file for connection events
    FILE *f = fopen(LIBRESPOT_EVENT_FILE, "r");
    if (!f) return false;

    char line[256];
    bool connected = false;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        // librespot events: session_connected, playing, paused, stopped, etc.
        if (strstr(line, "session_connected") || strstr(line, "playing")) {
            connected = true;
        }
    }
    fclose(f);

    if (connected) {
        g_state = SP_STATE_CONNECTED;
        printf("[SPOTIFY] Device connected via Spotify Connect!\n");
        return true;
    }

    return false;
}

// ============================================================================
// Web API
// ============================================================================

bool spotify_api_authenticate(void) {
    if (g_client_id[0] == '\0' || g_client_secret[0] == '\0') {
        snprintf(g_error, sizeof(g_error), "No API credentials configured");
        return false;
    }

    // Client Credentials flow - no user auth needed for search
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -s -X POST 'https://accounts.spotify.com/api/token' "
        "-d 'grant_type=client_credentials' "
        "-d 'client_id=%s' "
        "-d 'client_secret=%s' "
        "> %s 2>/dev/null",
        g_client_id, g_client_secret, TEMP_API_FILE);

    printf("[SPOTIFY] Authenticating with Web API...\n");
    int ret = system(cmd);

    if (ret != 0) {
        snprintf(g_error, sizeof(g_error), "API auth failed (network error?)");
        return false;
    }

    // Read response
    FILE *f = fopen(TEMP_API_FILE, "r");
    if (!f) {
        snprintf(g_error, sizeof(g_error), "Failed to read auth response");
        return false;
    }

    char buf[2048];
    size_t read = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    unlink(TEMP_API_FILE);
    buf[read] = '\0';

    // Parse token
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        snprintf(g_error, sizeof(g_error), "Invalid auth response");
        return false;
    }

    cJSON *token = cJSON_GetObjectItem(json, "access_token");
    cJSON *expires = cJSON_GetObjectItem(json, "expires_in");

    if (token && token->valuestring) {
        strncpy(g_access_token, token->valuestring, sizeof(g_access_token) - 1);
        int exp_sec = (expires && cJSON_IsNumber(expires)) ? (int)expires->valuedouble : 3600;
        g_token_expires = time(NULL) + exp_sec - 60;  // Refresh 60s before expiry
        printf("[SPOTIFY] API token obtained (expires in %ds)\n", exp_sec);
        cJSON_Delete(json);
        return true;
    }

    // Check for error
    cJSON *err = cJSON_GetObjectItem(json, "error");
    if (err && err->valuestring) {
        snprintf(g_error, sizeof(g_error), "Auth error: %s", err->valuestring);
    } else {
        snprintf(g_error, sizeof(g_error), "Auth failed (invalid response)");
    }

    cJSON_Delete(json);
    return false;
}

/**
 * Ensure we have a valid API token (refresh if expired)
 */
static bool ensure_api_token(void) {
    if (g_access_token[0] != '\0' && time(NULL) < g_token_expires) {
        return true;  // Token still valid
    }
    return spotify_api_authenticate();
}

/**
 * URL encode a string (same as youtube.c)
 */
static void url_encode(const char *src, char *dst, size_t dst_size) {
    static const char *hex = "0123456789ABCDEF";
    size_t j = 0;

    for (size_t i = 0; src[i] && j < dst_size - 4; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[j++] = c;
        } else if (c == ' ') {
            dst[j++] = '+';
        } else {
            dst[j++] = '%';
            dst[j++] = hex[c >> 4];
            dst[j++] = hex[c & 0x0F];
        }
    }
    dst[j] = '\0';
}

int spotify_search(const char *query, SpotifyTrack *results, int max_results) {
    if (!query || !results || max_results <= 0) {
        snprintf(g_error, sizeof(g_error), "Invalid search parameters");
        return -1;
    }

    if (max_results > SPOTIFY_MAX_RESULTS) {
        max_results = SPOTIFY_MAX_RESULTS;
    }

    g_error[0] = '\0';

    // Ensure API token
    if (!ensure_api_token()) {
        return -1;
    }

    // URL encode query
    char encoded_query[512];
    url_encode(query, encoded_query, sizeof(encoded_query));

    // Build search request
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "curl -s 'https://api.spotify.com/v1/search?q=%s&type=track&limit=%d' "
        "-H 'Authorization: Bearer %s' "
        "> %s 2>/dev/null",
        encoded_query, max_results, g_access_token, TEMP_API_FILE);

    printf("[SPOTIFY] Searching: %s\n", query);
    int ret = system(cmd);

    if (ret != 0) {
        snprintf(g_error, sizeof(g_error), "Search failed (network error?)");
        unlink(TEMP_API_FILE);
        return -1;
    }

    // Read response
    FILE *f = fopen(TEMP_API_FILE, "r");
    if (!f) {
        snprintf(g_error, sizeof(g_error), "Failed to read search results");
        return -1;
    }

    // Read entire file (search results can be large)
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 65536) {
        fclose(f);
        unlink(TEMP_API_FILE);
        snprintf(g_error, sizeof(g_error), "Invalid search response");
        return -1;
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        unlink(TEMP_API_FILE);
        return -1;
    }

    size_t bytes_read = fread(buf, 1, size, f);
    fclose(f);
    unlink(TEMP_API_FILE);
    buf[bytes_read] = '\0';

    // Parse JSON response
    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json) {
        snprintf(g_error, sizeof(g_error), "Invalid search JSON");
        return -1;
    }

    // Check for API error
    cJSON *error = cJSON_GetObjectItem(json, "error");
    if (error) {
        cJSON *msg = cJSON_GetObjectItem(error, "message");
        snprintf(g_error, sizeof(g_error), "%s",
                 (msg && msg->valuestring) ? msg->valuestring : "API error");
        cJSON_Delete(json);
        // Token might be expired
        g_access_token[0] = '\0';
        return -1;
    }

    // Navigate: tracks.items[]
    cJSON *tracks = cJSON_GetObjectItem(json, "tracks");
    cJSON *items = tracks ? cJSON_GetObjectItem(tracks, "items") : NULL;

    if (!items || !cJSON_IsArray(items)) {
        cJSON_Delete(json);
        snprintf(g_error, sizeof(g_error), "No results for '%s'", query);
        return 0;
    }

    int count = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, items) {
        if (count >= max_results) break;

        cJSON *uri = cJSON_GetObjectItem(item, "uri");
        cJSON *name = cJSON_GetObjectItem(item, "name");
        cJSON *duration = cJSON_GetObjectItem(item, "duration_ms");

        if (!uri || !uri->valuestring || !name || !name->valuestring) continue;

        SpotifyTrack *t = &results[count];
        memset(t, 0, sizeof(SpotifyTrack));

        strncpy(t->uri, uri->valuestring, sizeof(t->uri) - 1);
        strncpy(t->title, name->valuestring, sizeof(t->title) - 1);

        // Artist name (first artist in array)
        cJSON *artists = cJSON_GetObjectItem(item, "artists");
        if (artists && cJSON_IsArray(artists)) {
            cJSON *first = cJSON_GetArrayItem(artists, 0);
            if (first) {
                cJSON *artist_name = cJSON_GetObjectItem(first, "name");
                if (artist_name && artist_name->valuestring) {
                    strncpy(t->artist, artist_name->valuestring, sizeof(t->artist) - 1);
                }
            }
        }

        // Album name
        cJSON *album = cJSON_GetObjectItem(item, "album");
        if (album) {
            cJSON *album_name = cJSON_GetObjectItem(album, "name");
            if (album_name && album_name->valuestring) {
                strncpy(t->album, album_name->valuestring, sizeof(t->album) - 1);
            }
        }

        // Duration
        if (duration && cJSON_IsNumber(duration)) {
            t->duration_ms = (int)duration->valuedouble;
        }

        count++;
        printf("[SPOTIFY] Result %d: %s - %s (%dms)\n",
               count, t->artist, t->title, t->duration_ms);
    }

    cJSON_Delete(json);

    if (count == 0) {
        snprintf(g_error, sizeof(g_error), "No results for '%s'", query);
    }

    printf("[SPOTIFY] Found %d results\n", count);
    return count;
}

bool spotify_play_track(const char *uri) {
    if (!uri || g_state < SP_STATE_CONNECTED) {
        snprintf(g_error, sizeof(g_error), "Not connected");
        return false;
    }

    // librespot doesn't have a direct play-URI command via CLI.
    // When using Spotify Connect, the phone app controls what plays.
    // For direct playback, we'd use the Web API's player endpoint.
    //
    // For now, this is a placeholder - the phone controls playback
    // and librespot streams whatever the phone tells it to play.
    // The PCM audio flows through the FIFO pipe regardless.

    printf("[SPOTIFY] Play request: %s (controlled via phone)\n", uri);

    // Store current track info
    g_has_current_track = true;
    g_position_ms = 0;

    // Find the track in recent search results or set from URI
    // (Track metadata comes from the search results, not librespot)

    g_state = SP_STATE_PLAYING;
    return true;
}

void spotify_toggle_pause(void) {
    if (g_state == SP_STATE_PLAYING) {
        g_state = SP_STATE_PAUSED;
        // Send SIGUSR1 to librespot to pause (if supported)
        if (g_librespot_pid > 0) {
            kill(g_librespot_pid, SIGUSR1);
        }
    } else if (g_state == SP_STATE_PAUSED) {
        g_state = SP_STATE_PLAYING;
        if (g_librespot_pid > 0) {
            kill(g_librespot_pid, SIGUSR1);
        }
    }
}

void spotify_stop_playback(void) {
    g_state = SP_STATE_CONNECTED;
    g_has_current_track = false;
    g_position_ms = 0;
}

bool spotify_is_streaming(void) {
    return g_state == SP_STATE_PLAYING;
}

const SpotifyTrack* spotify_get_current_track(void) {
    return g_has_current_track ? &g_current_track : NULL;
}

int spotify_get_position_ms(void) {
    return g_position_ms;
}

const char* spotify_get_error(void) {
    return g_error[0] ? g_error : NULL;
}

void spotify_format_duration(int duration_ms, char *buffer) {
    if (duration_ms <= 0) {
        strcpy(buffer, "0:00");
        return;
    }

    int total_sec = duration_ms / 1000;
    int hours = total_sec / 3600;
    int mins = (total_sec % 3600) / 60;
    int secs = total_sec % 60;

    if (hours > 0) {
        sprintf(buffer, "%d:%02d:%02d", hours, mins, secs);
    } else {
        sprintf(buffer, "%d:%02d", mins, secs);
    }
}

const char* spotify_get_cache_dir(void) {
    return g_cache_dir;
}

bool spotify_has_cached_credentials(void) {
    char cred_path[512];
    snprintf(cred_path, sizeof(cred_path), "%s/credentials.json", g_cache_dir);
    return access(cred_path, F_OK) == 0;
}
