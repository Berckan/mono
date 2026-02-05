/**
 * YouTube Music Integration Implementation
 *
 * Uses yt-dlp CLI for YouTube search and download.
 * Follows patterns from metadata.c for CLI execution and JSON parsing.
 */

#include "youtube.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// Paths for yt-dlp binary (in order of preference)
#define YTDLP_BUNDLED_REL "./bin/yt-dlp"           // When running from Mono.pak/
#define YTDLP_BUNDLED_ABS "Mono.pak/bin/yt-dlp"    // When running from parent dir
#define YTDLP_SYSTEM      "yt-dlp"                 // System-wide installation

// File locations
#define TEMP_SEARCH_FILE "/tmp/mono_yt_search.json"
#define DEFAULT_DOWNLOAD_DIR "/mnt/SDCARD/Music/YouTube"

// Current state
static bool g_available = false;
static char g_ytdlp_path[256] = {0};
static char g_download_dir[256] = DEFAULT_DOWNLOAD_DIR;
static char g_download_file[512] = {0};
static char g_error[256] = {0};

/**
 * Sanitize filename - remove invalid characters
 */
static void sanitize_filename(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        char c = src[i];
        // Replace invalid filename characters with underscore
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
        dst[j++] = c;
    }
    dst[j] = '\0';

    // Trim trailing spaces/dots (Windows compatibility)
    while (j > 0 && (dst[j-1] == ' ' || dst[j-1] == '.')) {
        dst[--j] = '\0';
    }
}

/**
 * URL encode a string for use in search query
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

/**
 * Check if a file exists and is executable
 */
static bool file_executable(const char *path) {
    return access(path, X_OK) == 0;
}

/**
 * Cleanup temp search files (not downloaded music - that's permanent)
 */
static void cleanup_temp_files(void) {
    unlink(TEMP_SEARCH_FILE);
}

void youtube_init(void) {
    g_available = false;
    g_ytdlp_path[0] = '\0';
    g_download_file[0] = '\0';
    g_error[0] = '\0';

    // Create download directory if it doesn't exist
    mkdir(g_download_dir, 0755);

    // Check for bundled yt-dlp (relative path - when running from Mono.pak/)
    if (file_executable(YTDLP_BUNDLED_REL)) {
        strncpy(g_ytdlp_path, YTDLP_BUNDLED_REL, sizeof(g_ytdlp_path) - 1);
        g_available = true;
        printf("[YOUTUBE] Using bundled yt-dlp: %s\n", g_ytdlp_path);
        return;
    }

    // Check for bundled yt-dlp (absolute path - when running from parent dir)
    if (file_executable(YTDLP_BUNDLED_ABS)) {
        strncpy(g_ytdlp_path, YTDLP_BUNDLED_ABS, sizeof(g_ytdlp_path) - 1);
        g_available = true;
        printf("[YOUTUBE] Using bundled yt-dlp: %s\n", g_ytdlp_path);
        return;
    }

    // Check for system yt-dlp (useful for development)
    char which_cmd[64];
    snprintf(which_cmd, sizeof(which_cmd), "which %s >/dev/null 2>&1", YTDLP_SYSTEM);
    if (system(which_cmd) == 0) {
        strncpy(g_ytdlp_path, YTDLP_SYSTEM, sizeof(g_ytdlp_path) - 1);
        g_available = true;
        printf("[YOUTUBE] Using system yt-dlp\n");
        return;
    }

    printf("[YOUTUBE] yt-dlp not found - YouTube features disabled\n");
    snprintf(g_error, sizeof(g_error), "yt-dlp not found");
}

void youtube_cleanup(void) {
    cleanup_temp_files();
    g_download_file[0] = '\0';
    printf("[YOUTUBE] Cleanup complete\n");
}

bool youtube_is_available(void) {
    return g_available;
}

int youtube_search(const char *query, YouTubeResult *results, int max_results) {
    if (!g_available || !query || !results || max_results <= 0) {
        snprintf(g_error, sizeof(g_error), "Invalid parameters");
        return -1;
    }

    if (max_results > YOUTUBE_MAX_RESULTS) {
        max_results = YOUTUBE_MAX_RESULTS;
    }

    g_error[0] = '\0';

    // URL encode the query
    char encoded_query[512];
    url_encode(query, encoded_query, sizeof(encoded_query));

    // Build yt-dlp search command
    // Use --flat-playlist to get results fast without downloading
    // ytsearch10: searches YouTube and returns 10 results
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "%s --flat-playlist --dump-json 'ytsearch%d:%s' > %s 2>/dev/null",
        g_ytdlp_path, max_results, query, TEMP_SEARCH_FILE);

    printf("[YOUTUBE] Searching: %s\n", query);
    int ret = system(cmd);

    if (ret != 0) {
        snprintf(g_error, sizeof(g_error), "Search failed (network error?)");
        fprintf(stderr, "[YOUTUBE] Search command failed with code %d\n", ret);
        unlink(TEMP_SEARCH_FILE);
        return -1;
    }

    // Read and parse JSON output (one JSON object per line)
    FILE *f = fopen(TEMP_SEARCH_FILE, "r");
    if (!f) {
        snprintf(g_error, sizeof(g_error), "Failed to read search results");
        return -1;
    }

    int count = 0;
    char line[4096];

    while (fgets(line, sizeof(line), f) && count < max_results) {
        // Parse each line as JSON
        cJSON *json = cJSON_Parse(line);
        if (!json) continue;

        // Extract fields
        cJSON *id = cJSON_GetObjectItem(json, "id");
        cJSON *title = cJSON_GetObjectItem(json, "title");
        cJSON *channel = cJSON_GetObjectItem(json, "channel");
        cJSON *uploader = cJSON_GetObjectItem(json, "uploader");  // Fallback
        cJSON *duration = cJSON_GetObjectItem(json, "duration");

        if (id && id->valuestring && title && title->valuestring) {
            YouTubeResult *r = &results[count];
            memset(r, 0, sizeof(YouTubeResult));

            strncpy(r->id, id->valuestring, sizeof(r->id) - 1);
            strncpy(r->title, title->valuestring, sizeof(r->title) - 1);

            // Channel name (try channel first, then uploader)
            if (channel && channel->valuestring) {
                strncpy(r->channel, channel->valuestring, sizeof(r->channel) - 1);
            } else if (uploader && uploader->valuestring) {
                strncpy(r->channel, uploader->valuestring, sizeof(r->channel) - 1);
            }

            // Duration (may be null for live streams)
            if (duration && cJSON_IsNumber(duration)) {
                r->duration_sec = (int)duration->valuedouble;
            }

            count++;
            printf("[YOUTUBE] Result %d: %s - %s (%ds)\n",
                   count, r->channel, r->title, r->duration_sec);
        }

        cJSON_Delete(json);
    }

    fclose(f);
    unlink(TEMP_SEARCH_FILE);

    if (count == 0) {
        snprintf(g_error, sizeof(g_error), "No results for '%s'", query);
    }

    printf("[YOUTUBE] Found %d results\n", count);
    return count;
}

const char* youtube_download(const char *video_id, YouTubeProgressCallback progress_cb) {
    if (!g_available || !video_id) {
        snprintf(g_error, sizeof(g_error), "Invalid parameters");
        return NULL;
    }

    g_error[0] = '\0';

    // Don't delete previous file - downloads are permanent now
    g_download_file[0] = '\0';

    // Check if already downloaded (cache hit) - try common audio extensions
    const char *cache_exts[] = {".webm", ".ogg", ".opus", ".m4a", NULL};
    char cache_path[512];
    for (int i = 0; cache_exts[i]; i++) {
        snprintf(cache_path, sizeof(cache_path),
                 "%s/%s%s", g_download_dir, video_id, cache_exts[i]);
        if (access(cache_path, F_OK) == 0) {
            strncpy(g_download_file, cache_path, sizeof(g_download_file) - 1);
            printf("[YOUTUBE] Using cached file: %s\n", g_download_file);
            return g_download_file;
        }
    }

    if (progress_cb) {
        progress_cb(0, "Starting download...");
    }

    // Build yt-dlp download command
    // -f: Select best audio format (no ffmpeg needed)
    // --progress --newline: Machine-readable progress output
    // -o: Output template with extension placeholder
    // Note: We DON'T use -x (extract audio) because it requires ffmpeg
    //       Instead we download the best audio-only format directly
    char cmd[1024];
    char output_template[512];
    snprintf(output_template, sizeof(output_template),
             "%s/%s.%%(ext)s", g_download_dir, video_id);

    snprintf(cmd, sizeof(cmd),
        "%s -f 'bestaudio[ext=webm]/bestaudio[ext=ogg]/bestaudio' "
        "--no-playlist --progress --newline "
        "-o '%s' "
        "'https://www.youtube.com/watch?v=%s' "
        "2>&1",
        g_ytdlp_path, output_template, video_id);

    // Update expected file path (will check multiple extensions after download)
    snprintf(g_download_file, sizeof(g_download_file),
             "%s/%s.webm", g_download_dir, video_id);

    printf("[YOUTUBE] Downloading: %s\n", video_id);
    printf("[YOUTUBE] Command: %s\n", cmd);

    // Execute download
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        snprintf(g_error, sizeof(g_error), "Failed to start download");
        return NULL;
    }

    // Read output for progress (basic parsing)
    char line[256];
    int last_percent = 0;

    while (fgets(line, sizeof(line), pipe)) {
        // Remove trailing newline for logging
        line[strcspn(line, "\n")] = '\0';

        // Log all output for debugging
        printf("[YOUTUBE] yt-dlp: %s\n", line);

        // Look for error messages
        if (strstr(line, "ERROR") || strstr(line, "error")) {
            // Store error message for user
            strncpy(g_error, line, sizeof(g_error) - 1);
        }

        // Look for download progress
        // yt-dlp output with --progress --newline: [download]  50.0% of 5.00MiB
        char *pct = strstr(line, "%");
        if (pct) {
            // Find the number before %
            char *start = pct - 1;
            while (start > line && (*start == '.' || (*start >= '0' && *start <= '9'))) {
                start--;
            }
            start++;

            int percent = (int)atof(start);
            if (percent != last_percent && progress_cb) {
                char status[64];
                snprintf(status, sizeof(status), "Downloading... %d%%", percent);
                if (!progress_cb(percent, status)) {
                    // Cancelled
                    pclose(pipe);
                    unlink(g_download_file);
                    g_download_file[0] = '\0';
                    snprintf(g_error, sizeof(g_error), "Download cancelled");
                    return NULL;
                }
                last_percent = percent;
            }
        }
    }

    int status = pclose(pipe);

    // Check if file was created (try multiple extensions)
    // yt-dlp might output different formats depending on source
    if (access(g_download_file, F_OK) != 0) {
        // Try alternative extensions
        const char *alt_exts[] = {".webm", ".ogg", ".opus", ".m4a", NULL};
        char alt_path[512];
        bool found = false;

        // Build base path without extension
        char base_path[512];
        snprintf(base_path, sizeof(base_path), "%s/%s", g_download_dir, video_id);

        for (int i = 0; alt_exts[i]; i++) {
            snprintf(alt_path, sizeof(alt_path), "%s%s", base_path, alt_exts[i]);
            if (access(alt_path, F_OK) == 0) {
                strncpy(g_download_file, alt_path, sizeof(g_download_file) - 1);
                printf("[YOUTUBE] Downloaded (%s): %s\n", alt_exts[i] + 1, g_download_file);
                found = true;
                break;
            }
        }

        if (!found) {
            snprintf(g_error, sizeof(g_error), "Download failed (exit: %d)", status);
            fprintf(stderr, "[YOUTUBE] Download failed, exit code: %d\n", status);
            g_download_file[0] = '\0';
            return NULL;
        }
    }

    if (progress_cb) {
        progress_cb(100, "Download complete!");
    }

    printf("[YOUTUBE] Downloaded: %s\n", g_download_file);
    return g_download_file;
}

const char* youtube_get_temp_path(void) {
    return g_download_file[0] ? g_download_file : NULL;
}

const char* youtube_get_error(void) {
    return g_error[0] ? g_error : NULL;
}

void youtube_format_duration(int duration_sec, char *buffer) {
    if (duration_sec <= 0) {
        strcpy(buffer, "LIVE");
        return;
    }

    int hours = duration_sec / 3600;
    int mins = (duration_sec % 3600) / 60;
    int secs = duration_sec % 60;

    if (hours > 0) {
        sprintf(buffer, "%d:%02d:%02d", hours, mins, secs);
    } else {
        sprintf(buffer, "%d:%02d", mins, secs);
    }
}

const char* youtube_get_download_dir(void) {
    return g_download_dir;
}
