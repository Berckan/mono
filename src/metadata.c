/**
 * Metadata Scanner Implementation
 *
 * Uses MusicBrainz API via curl CLI for metadata lookup.
 * Caches results in ~/.mono/metadata_cache.json
 */

#include "metadata.h"
#include "version.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

// Cache file location
#define CACHE_DIR_HOME ".mono"
#define CACHE_FILENAME "metadata_cache.json"
#define CACHE_BACKUP_FILENAME "metadata_cache.json.bak"
#define TEMP_FILE "/tmp/mono_mb_response.json"

// MusicBrainz API
#define MB_API_BASE "https://musicbrainz.org/ws/2/recording"
#define MB_USER_AGENT VERSION_USER_AGENT

// Rate limiting (1 request per second)
#define RATE_LIMIT_MS 1100

// Minimum confidence to auto-accept match
#define MIN_CONFIDENCE 60

// Cache storage
static cJSON *g_cache = NULL;
static char g_cache_path[512] = {0};
static int g_total_lookups = 0;
static bool g_cache_dirty = false;

// Audio file extensions
static const char *AUDIO_EXTENSIONS[] = {
    ".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aac", NULL
};

/**
 * URL encode a string for use in query parameters
 */
static void url_encode(const char *src, char *dst, size_t dst_size) {
    static const char *hex = "0123456789ABCDEF";
    size_t j = 0;

    for (size_t i = 0; src[i] && j < dst_size - 4; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
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
 * Extract search query from filename
 * "01. Suite-Pee.flac" -> "Suite-Pee"
 * "Artist - Title.mp3" -> "Artist Title"
 */
static void extract_search_query(const char *filepath, char *query, size_t size) {
    // Get filename without path
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;

    // Copy and work on it
    char temp[256];
    strncpy(temp, filename, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    // Remove extension
    char *dot = strrchr(temp, '.');
    if (dot) *dot = '\0';

    // Remove leading track number (01. or 01 - or just 01)
    char *start = temp;
    while (*start && (isdigit(*start) || *start == '.' || *start == '-' || *start == ' ')) {
        if (*start == '.' || (*start == '-' && start > temp)) {
            start++;
            while (*start == ' ') start++;
            break;
        }
        start++;
    }
    if (start == temp + strlen(temp)) start = temp;  // Fallback if all stripped

    // Copy result, replacing special chars with spaces
    size_t j = 0;
    for (size_t i = 0; start[i] && j < size - 1; i++) {
        char c = start[i];
        if (c == '-' || c == '_' || c == '(' || c == ')' || c == '[' || c == ']') {
            if (j > 0 && query[j-1] != ' ') query[j++] = ' ';
        } else {
            query[j++] = c;
        }
    }
    query[j] = '\0';

    // Trim trailing spaces
    while (j > 0 && query[j-1] == ' ') query[--j] = '\0';
}

/**
 * Check if file is an audio file
 */
static bool is_audio_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;

    for (int i = 0; AUDIO_EXTENSIONS[i]; i++) {
        if (strcasecmp(ext, AUDIO_EXTENSIONS[i]) == 0) return true;
    }
    return false;
}

/**
 * Get cache directory path
 */
static const char* get_cache_dir(void) {
    static char dir[512] = {0};
    if (dir[0]) return dir;

    const char *home = getenv("HOME");
    if (home) {
        snprintf(dir, sizeof(dir), "%s/%s", home, CACHE_DIR_HOME);
    } else {
        snprintf(dir, sizeof(dir), "/tmp/%s", CACHE_DIR_HOME);
    }
    return dir;
}

/**
 * Ensure cache directory exists
 */
static void ensure_cache_dir(void) {
    const char *dir = get_cache_dir();
    mkdir(dir, 0755);  // Ignore error if exists
}

/**
 * Load cache from disk
 */
static void load_cache(void) {
    if (g_cache) {
        cJSON_Delete(g_cache);
        g_cache = NULL;
    }

    snprintf(g_cache_path, sizeof(g_cache_path), "%s/%s", get_cache_dir(), CACHE_FILENAME);

    FILE *f = fopen(g_cache_path, "r");
    if (!f) {
        g_cache = cJSON_CreateObject();
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 10 * 1024 * 1024) {  // Max 10MB
        fclose(f);
        g_cache = cJSON_CreateObject();
        return;
    }

    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        g_cache = cJSON_CreateObject();
        return;
    }

    size_t read = fread(json, 1, size, f);
    json[read] = '\0';
    fclose(f);

    g_cache = cJSON_Parse(json);
    free(json);

    if (!g_cache) {
        g_cache = cJSON_CreateObject();
    }

    printf("[METADATA] Loaded cache: %d entries\n", cJSON_GetArraySize(g_cache));
}

/**
 * Save cache to disk
 */
static void save_cache(void) {
    if (!g_cache || !g_cache_dirty) return;

    ensure_cache_dir();

    char *json = cJSON_Print(g_cache);
    if (!json) return;

    FILE *f = fopen(g_cache_path, "w");
    if (f) {
        fputs(json, f);
        fclose(f);
        g_cache_dirty = false;
        printf("[METADATA] Saved cache: %d entries\n", cJSON_GetArraySize(g_cache));
    }

    free(json);
}

/**
 * Query MusicBrainz API using curl CLI
 */
static bool query_musicbrainz(const char *search_query, MetadataResult *result) {
    char encoded_query[512];
    url_encode(search_query, encoded_query, sizeof(encoded_query));

    // Build curl command
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -s -A '%s' '%s?query=%s&fmt=json&limit=3' -o '%s' 2>/dev/null",
        MB_USER_AGENT, MB_API_BASE, encoded_query, TEMP_FILE);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[METADATA] curl failed with code %d\n", ret);
        return false;
    }

    // Read response
    FILE *f = fopen(TEMP_FILE, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {
        fclose(f);
        unlink(TEMP_FILE);
        return false;
    }

    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        unlink(TEMP_FILE);
        return false;
    }

    size_t read = fread(json, 1, size, f);
    json[read] = '\0';
    fclose(f);
    unlink(TEMP_FILE);

    // Parse JSON
    cJSON *root = cJSON_Parse(json);
    free(json);

    if (!root) return false;

    bool found = false;
    cJSON *recordings = cJSON_GetObjectItem(root, "recordings");

    if (recordings && cJSON_IsArray(recordings) && cJSON_GetArraySize(recordings) > 0) {
        cJSON *first = cJSON_GetArrayItem(recordings, 0);

        // Get score (confidence)
        cJSON *score = cJSON_GetObjectItem(first, "score");
        int confidence = score ? score->valueint : 0;

        if (confidence >= MIN_CONFIDENCE) {
            // Get title
            cJSON *title = cJSON_GetObjectItem(first, "title");
            if (title && title->valuestring) {
                strncpy(result->title, title->valuestring, sizeof(result->title) - 1);
            }

            // Get artist (from artist-credit array)
            cJSON *artist_credit = cJSON_GetObjectItem(first, "artist-credit");
            if (artist_credit && cJSON_IsArray(artist_credit) && cJSON_GetArraySize(artist_credit) > 0) {
                cJSON *artist_obj = cJSON_GetArrayItem(artist_credit, 0);
                cJSON *artist = cJSON_GetObjectItem(artist_obj, "name");
                if (artist && artist->valuestring) {
                    strncpy(result->artist, artist->valuestring, sizeof(result->artist) - 1);
                }
            }

            // Get album (from releases array)
            cJSON *releases = cJSON_GetObjectItem(first, "releases");
            if (releases && cJSON_IsArray(releases) && cJSON_GetArraySize(releases) > 0) {
                cJSON *release = cJSON_GetArrayItem(releases, 0);
                cJSON *album = cJSON_GetObjectItem(release, "title");
                if (album && album->valuestring) {
                    strncpy(result->album, album->valuestring, sizeof(result->album) - 1);
                }
            }

            result->confidence = confidence;
            found = result->title[0] != '\0';
        }
    }

    cJSON_Delete(root);
    g_total_lookups++;

    return found;
}

/**
 * Add result to cache
 */
static void cache_result(const char *filepath, const MetadataResult *result) {
    if (!g_cache) return;

    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "title", result->title);
    cJSON_AddStringToObject(entry, "artist", result->artist);
    cJSON_AddStringToObject(entry, "album", result->album);
    cJSON_AddNumberToObject(entry, "confidence", result->confidence);

    // Remove old entry if exists
    cJSON_DeleteItemFromObject(g_cache, filepath);
    cJSON_AddItemToObject(g_cache, filepath, entry);

    g_cache_dirty = true;
}

// ============================================================================
// Public API
// ============================================================================

void metadata_init(void) {
    load_cache();
    g_total_lookups = 0;
}

void metadata_cleanup(void) {
    save_cache();
    if (g_cache) {
        cJSON_Delete(g_cache);
        g_cache = NULL;
    }
}

bool metadata_lookup(const char *filepath, MetadataResult *result) {
    if (!filepath || !result) return false;

    memset(result, 0, sizeof(MetadataResult));

    // Check cache first
    if (metadata_get_cached(filepath, result)) {
        return true;
    }

    // Extract search query from filename
    char query[256];
    extract_search_query(filepath, query, sizeof(query));

    if (strlen(query) < 2) return false;

    printf("[METADATA] Searching: %s\n", query);

    // Query MusicBrainz
    if (query_musicbrainz(query, result)) {
        cache_result(filepath, result);
        printf("[METADATA] Found: %s - %s (%d%%)\n",
               result->artist, result->title, result->confidence);
        return true;
    }

    return false;
}

int metadata_scan_folder(const char *folder_path, ScanProgressCallback progress_cb) {
    if (!folder_path) return 0;

    DIR *dir = opendir(folder_path);
    if (!dir) return 0;

    // Create backup before scanning (so user can restore if results are bad)
    metadata_backup_cache();

    // First pass: count audio files
    int total = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (is_audio_file(entry->d_name)) total++;
    }
    rewinddir(dir);

    if (total == 0) {
        closedir(dir);
        return 0;
    }

    // Second pass: scan files
    int current = 0;
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (!is_audio_file(entry->d_name)) continue;

        current++;

        // Build full path
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", folder_path, entry->d_name);

        // Progress callback
        if (progress_cb && !progress_cb(current, total, entry->d_name)) {
            break;  // Cancelled
        }

        // Skip if already cached
        if (metadata_has_cache(filepath)) {
            found++;
            continue;
        }

        // Lookup metadata
        MetadataResult result;
        if (metadata_lookup(filepath, &result)) {
            found++;
        }

        // Rate limiting (1 req/sec for MusicBrainz)
        usleep(RATE_LIMIT_MS * 1000);
    }

    closedir(dir);
    save_cache();  // Save after scan

    return found;
}

bool metadata_get_cached(const char *filepath, MetadataResult *result) {
    if (!g_cache || !filepath || !result) return false;

    cJSON *entry = cJSON_GetObjectItem(g_cache, filepath);
    if (!entry) return false;

    memset(result, 0, sizeof(MetadataResult));

    cJSON *title = cJSON_GetObjectItem(entry, "title");
    cJSON *artist = cJSON_GetObjectItem(entry, "artist");
    cJSON *album = cJSON_GetObjectItem(entry, "album");
    cJSON *confidence = cJSON_GetObjectItem(entry, "confidence");

    if (title && title->valuestring) {
        strncpy(result->title, title->valuestring, sizeof(result->title) - 1);
    }
    if (artist && artist->valuestring) {
        strncpy(result->artist, artist->valuestring, sizeof(result->artist) - 1);
    }
    if (album && album->valuestring) {
        strncpy(result->album, album->valuestring, sizeof(result->album) - 1);
    }
    if (confidence) {
        result->confidence = confidence->valueint;
    }

    return result->title[0] != '\0';
}

bool metadata_has_cache(const char *filepath) {
    if (!g_cache || !filepath) return false;
    return cJSON_HasObjectItem(g_cache, filepath);
}

void metadata_clear_cache(void) {
    if (g_cache) {
        cJSON_Delete(g_cache);
        g_cache = cJSON_CreateObject();
        g_cache_dirty = true;
        save_cache();
    }
}

bool metadata_backup_cache(void) {
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s/%s", get_cache_dir(), CACHE_BACKUP_FILENAME);

    // If cache file doesn't exist, nothing to backup
    FILE *src = fopen(g_cache_path, "r");
    if (!src) return false;

    FILE *dst = fopen(backup_path, "w");
    if (!dst) {
        fclose(src);
        return false;
    }

    // Copy file contents
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        fwrite(buf, 1, n, dst);
    }

    fclose(src);
    fclose(dst);

    printf("[METADATA] Backup created: %s\n", backup_path);
    return true;
}

bool metadata_restore_backup(void) {
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s/%s", get_cache_dir(), CACHE_BACKUP_FILENAME);

    FILE *src = fopen(backup_path, "r");
    if (!src) {
        fprintf(stderr, "[METADATA] No backup found\n");
        return false;
    }

    FILE *dst = fopen(g_cache_path, "w");
    if (!dst) {
        fclose(src);
        return false;
    }

    // Copy backup to cache
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        fwrite(buf, 1, n, dst);
    }

    fclose(src);
    fclose(dst);

    // Reload cache from restored file
    load_cache();

    printf("[METADATA] Restored from backup\n");
    return true;
}

bool metadata_has_backup(void) {
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s/%s", get_cache_dir(), CACHE_BACKUP_FILENAME);

    FILE *f = fopen(backup_path, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

void metadata_get_stats(int *total_cached, int *total_lookups) {
    if (total_cached) {
        *total_cached = g_cache ? cJSON_GetArraySize(g_cache) : 0;
    }
    if (total_lookups) {
        *total_lookups = g_total_lookups;
    }
}
