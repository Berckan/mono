/**
 * Self-Update Implementation
 *
 * Uses curl CLI to fetch GitHub releases API and download binary.
 * Follows patterns from metadata.c and youtube.c.
 */

#include "update.h"
#include "version.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>  // For _NSGetExecutablePath
#endif

// Temporary file paths
#define TEMP_API_RESPONSE "/tmp/mono_update_api.json"
#define TEMP_BINARY       "/tmp/mono_update_binary"
#define TEMP_ZIP          "/tmp/mono_update.zip"
#define TEMP_EXTRACT_DIR  "/tmp/mono_extract"
#define BACKUP_SUFFIX     ".bak"

// GitHub API endpoint
#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_REPO_OWNER "/" GITHUB_REPO_NAME "/releases/latest"

// Timeout for curl operations (seconds)
#define CURL_TIMEOUT 30

// State
static UpdateState g_state = UPDATE_IDLE;
static UpdateInfo g_info = {0};
static char g_error[256] = {0};
static int g_progress = 0;

// Binary path (set at runtime)
static char g_binary_path[512] = {0};

// Whether current download is a zip (vs bare binary)
static bool g_is_zip = false;

/**
 * Compare version strings
 * Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
static int compare_versions(const char *v1, const char *v2) {
    // Skip leading 'v' if present
    if (v1[0] == 'v' || v1[0] == 'V') v1++;
    if (v2[0] == 'v' || v2[0] == 'V') v2++;

    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;

    sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2);

    if (major1 != major2) return (major1 < major2) ? -1 : 1;
    if (minor1 != minor2) return (minor1 < minor2) ? -1 : 1;
    if (patch1 != patch2) return (patch1 < patch2) ? -1 : 1;
    return 0;
}

/**
 * Get the path to the current binary
 */
static const char* get_binary_path(void) {
    if (g_binary_path[0]) return g_binary_path;

#ifdef __APPLE__
    // macOS: use _NSGetExecutablePath or just build/mono for dev
    uint32_t size = sizeof(g_binary_path);
    if (_NSGetExecutablePath(g_binary_path, &size) != 0) {
        snprintf(g_binary_path, sizeof(g_binary_path), "./build/mono");
    }
#else
    // Linux: read /proc/self/exe
    ssize_t len = readlink("/proc/self/exe", g_binary_path, sizeof(g_binary_path) - 1);
    if (len > 0) {
        g_binary_path[len] = '\0';
    } else {
        // Fallback for Trimui
        snprintf(g_binary_path, sizeof(g_binary_path),
                 "/mnt/SDCARD/Tools/tg5040/Mono.pak/bin/mono");
    }
#endif

    printf("[UPDATE] Binary path: %s\n", g_binary_path);
    return g_binary_path;
}

void update_init(void) {
    g_state = UPDATE_IDLE;
    memset(&g_info, 0, sizeof(g_info));
    g_error[0] = '\0';
    g_progress = 0;

    // Initialize binary path
    get_binary_path();
}

void update_cleanup(void) {
    // Clean up temp files
    unlink(TEMP_API_RESPONSE);
    unlink(TEMP_BINARY);
    unlink(TEMP_ZIP);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", TEMP_EXTRACT_DIR);
    system(cmd);
}

void update_check(void) {
    g_state = UPDATE_CHECKING;
    g_error[0] = '\0';
    memset(&g_info, 0, sizeof(g_info));

    printf("[UPDATE] Checking for updates...\n");
    printf("[UPDATE] Current version: %s\n", VERSION);
}

bool update_check_complete(void) {
    if (g_state != UPDATE_CHECKING) return true;

    // Execute curl to fetch GitHub API
    // Note: -k skips SSL verification (Trimui lacks updated CA certs)
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -s -k -m %d -A '%s' "
        "-H 'Accept: application/vnd.github.v3+json' "
        "'%s' -o '%s' 2>/dev/null",
        CURL_TIMEOUT, VERSION_USER_AGENT, GITHUB_API_URL, TEMP_API_RESPONSE);

    int ret = system(cmd);
    if (ret != 0) {
        snprintf(g_error, sizeof(g_error), "Network error (curl failed)");
        g_state = UPDATE_ERROR;
        printf("[UPDATE] curl failed with code %d\n", ret);
        return true;
    }

    // Read response file
    FILE *f = fopen(TEMP_API_RESPONSE, "r");
    if (!f) {
        snprintf(g_error, sizeof(g_error), "Failed to read API response");
        g_state = UPDATE_ERROR;
        return true;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 100 * 1024) {  // Max 100KB response
        fclose(f);
        snprintf(g_error, sizeof(g_error), "Invalid API response");
        g_state = UPDATE_ERROR;
        return true;
    }

    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        snprintf(g_error, sizeof(g_error), "Out of memory");
        g_state = UPDATE_ERROR;
        return true;
    }

    size_t read = fread(json, 1, size, f);
    json[read] = '\0';
    fclose(f);
    unlink(TEMP_API_RESPONSE);

    // Parse JSON response
    cJSON *root = cJSON_Parse(json);
    free(json);

    if (!root) {
        snprintf(g_error, sizeof(g_error), "Failed to parse API response");
        g_state = UPDATE_ERROR;
        return true;
    }

    // Check for API error
    cJSON *message = cJSON_GetObjectItem(root, "message");
    if (message && cJSON_IsString(message)) {
        snprintf(g_error, sizeof(g_error), "GitHub: %s", message->valuestring);
        cJSON_Delete(root);
        g_state = UPDATE_ERROR;
        return true;
    }

    // Extract version (tag_name)
    cJSON *tag_name = cJSON_GetObjectItem(root, "tag_name");
    if (!tag_name || !cJSON_IsString(tag_name)) {
        snprintf(g_error, sizeof(g_error), "No version in response");
        cJSON_Delete(root);
        g_state = UPDATE_ERROR;
        return true;
    }

    strncpy(g_info.version, tag_name->valuestring, sizeof(g_info.version) - 1);
    printf("[UPDATE] Latest version: %s\n", g_info.version);

    // Compare versions
    if (compare_versions(g_info.version, VERSION) <= 0) {
        printf("[UPDATE] Already up to date\n");
        cJSON_Delete(root);
        g_state = UPDATE_UP_TO_DATE;
        return true;
    }

    // Extract changelog (body)
    cJSON *body = cJSON_GetObjectItem(root, "body");
    if (body && cJSON_IsString(body)) {
        strncpy(g_info.changelog, body->valuestring, sizeof(g_info.changelog) - 1);
    }

    // Find binary asset in assets array
    cJSON *assets = cJSON_GetObjectItem(root, "assets");
    if (!assets || !cJSON_IsArray(assets)) {
        snprintf(g_error, sizeof(g_error), "No assets in release");
        cJSON_Delete(root);
        g_state = UPDATE_ERROR;
        return true;
    }

    bool found_binary = false;
    g_is_zip = false;
    int asset_count = cJSON_GetArraySize(assets);

    // First pass: look for zip release (v1.9.0+ format)
    for (int i = 0; i < asset_count; i++) {
        cJSON *asset = cJSON_GetArrayItem(assets, i);
        cJSON *name = cJSON_GetObjectItem(asset, "name");

        if (name && cJSON_IsString(name) &&
            (strcmp(name->valuestring, "mono-release.zip") == 0 ||
             strcmp(name->valuestring, "Mono.pak.zip") == 0)) {
            cJSON *url = cJSON_GetObjectItem(asset, "browser_download_url");
            cJSON *size_obj = cJSON_GetObjectItem(asset, "size");

            if (url && cJSON_IsString(url)) {
                strncpy(g_info.download_url, url->valuestring, sizeof(g_info.download_url) - 1);
                found_binary = true;
                g_is_zip = true;
            }
            if (size_obj && cJSON_IsNumber(size_obj)) {
                g_info.size_bytes = (size_t)size_obj->valuedouble;
            }
            break;
        }
    }

    // Second pass: fallback to bare binary (v1.7.0 format)
    if (!found_binary) {
        for (int i = 0; i < asset_count; i++) {
            cJSON *asset = cJSON_GetArrayItem(assets, i);
            cJSON *name = cJSON_GetObjectItem(asset, "name");

            if (name && cJSON_IsString(name) && strcmp(name->valuestring, "mono") == 0) {
                cJSON *url = cJSON_GetObjectItem(asset, "browser_download_url");
                cJSON *size_obj = cJSON_GetObjectItem(asset, "size");

                if (url && cJSON_IsString(url)) {
                    strncpy(g_info.download_url, url->valuestring, sizeof(g_info.download_url) - 1);
                    found_binary = true;
                    g_is_zip = false;
                }
                if (size_obj && cJSON_IsNumber(size_obj)) {
                    g_info.size_bytes = (size_t)size_obj->valuedouble;
                }
                break;
            }
        }
    }

    cJSON_Delete(root);

    if (!found_binary) {
        snprintf(g_error, sizeof(g_error), "Binary not found in release");
        g_state = UPDATE_ERROR;
        return true;
    }

    printf("[UPDATE] Update available: %s (%zu bytes)\n", g_info.version, g_info.size_bytes);
    g_state = UPDATE_AVAILABLE;
    return true;
}

void update_download(void) {
    if (g_state != UPDATE_AVAILABLE) return;

    g_state = UPDATE_DOWNLOADING;
    g_progress = 0;
    g_error[0] = '\0';

    printf("[UPDATE] Starting download: %s\n", g_info.download_url);
}

bool update_download_complete(void) {
    if (g_state != UPDATE_DOWNLOADING) return true;

    // Download with curl, showing progress
    // Use -# for progress bar output, parse it for percentage
    // Note: -k skips SSL verification (Trimui lacks updated CA certs)
    const char *dl_path = g_is_zip ? TEMP_ZIP : TEMP_BINARY;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -L -k -m 120 -A '%s' "
        "--progress-bar "
        "-o '%s' "
        "'%s' 2>&1",
        VERSION_USER_AGENT, dl_path, g_info.download_url);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        snprintf(g_error, sizeof(g_error), "Failed to start download");
        g_state = UPDATE_ERROR;
        return true;
    }

    char line[256];
    while (fgets(line, sizeof(line), pipe)) {
        // Parse curl progress bar output: "###  15.0%"
        char *pct = strstr(line, "%");
        if (pct) {
            char *start = pct - 1;
            while (start > line && (*start == '.' || (*start >= '0' && *start <= '9'))) {
                start--;
            }
            start++;
            g_progress = (int)atof(start);
            if (g_progress > 100) g_progress = 100;
        }
    }

    int status = pclose(pipe);

    // Verify download
    struct stat st;
    if (stat(dl_path, &st) != 0) {
        snprintf(g_error, sizeof(g_error), "Download failed - file not created");
        g_state = UPDATE_ERROR;
        return true;
    }

    // Verify size if we know it
    if (g_info.size_bytes > 0 && (size_t)st.st_size != g_info.size_bytes) {
        snprintf(g_error, sizeof(g_error), "Size mismatch: got %ld, expected %zu",
                 (long)st.st_size, g_info.size_bytes);
        unlink(dl_path);
        g_state = UPDATE_ERROR;
        return true;
    }

    if (status != 0) {
        snprintf(g_error, sizeof(g_error), "Download incomplete (curl error)");
        unlink(dl_path);
        g_state = UPDATE_ERROR;
        return true;
    }

    printf("[UPDATE] Download complete: %ld bytes\n", (long)st.st_size);
    g_progress = 100;

    // Extract binary from zip if needed
    if (g_is_zip) {
        printf("[UPDATE] Extracting binary from zip...\n");

        // Clean up and create extraction dir
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", TEMP_EXTRACT_DIR, TEMP_EXTRACT_DIR);
        system(cmd);

        // Try flat structure first (v1.9.1+: bin/mono at root of zip)
        snprintf(cmd, sizeof(cmd), "unzip -o '%s' 'bin/mono' -d '%s' 2>/dev/null", TEMP_ZIP, TEMP_EXTRACT_DIR);
        int extract_ret = system(cmd);

        if (extract_ret != 0) {
            // Try nested structure (v1.9.0: Mono.pak/bin/mono)
            snprintf(cmd, sizeof(cmd), "unzip -o '%s' 'Mono.pak/bin/mono' -d '%s' 2>/dev/null", TEMP_ZIP, TEMP_EXTRACT_DIR);
            extract_ret = system(cmd);
        }

        if (extract_ret != 0) {
            snprintf(g_error, sizeof(g_error), "Failed to extract binary from zip");
            unlink(TEMP_ZIP);
            snprintf(cmd, sizeof(cmd), "rm -rf '%s'", TEMP_EXTRACT_DIR);
            system(cmd);
            g_state = UPDATE_ERROR;
            return true;
        }

        // Find and move the extracted binary to TEMP_BINARY
        // Try flat path first, then nested
        bool extracted = false;
        char extract_path[512];

        snprintf(extract_path, sizeof(extract_path), "%s/bin/mono", TEMP_EXTRACT_DIR);
        if (stat(extract_path, &st) == 0) {
            extracted = true;
        }

        if (!extracted) {
            snprintf(extract_path, sizeof(extract_path), "%s/Mono.pak/bin/mono", TEMP_EXTRACT_DIR);
            if (stat(extract_path, &st) == 0) {
                extracted = true;
            }
        }

        if (!extracted) {
            snprintf(g_error, sizeof(g_error), "Binary not found in extracted zip");
            unlink(TEMP_ZIP);
            snprintf(cmd, sizeof(cmd), "rm -rf '%s'", TEMP_EXTRACT_DIR);
            system(cmd);
            g_state = UPDATE_ERROR;
            return true;
        }

        snprintf(cmd, sizeof(cmd), "mv '%s' '%s'", extract_path, TEMP_BINARY);
        if (system(cmd) != 0) {
            snprintf(g_error, sizeof(g_error), "Failed to move extracted binary");
            unlink(TEMP_ZIP);
            snprintf(cmd, sizeof(cmd), "rm -rf '%s'", TEMP_EXTRACT_DIR);
            system(cmd);
            g_state = UPDATE_ERROR;
            return true;
        }

        // Clean up zip and extract dir
        unlink(TEMP_ZIP);
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", TEMP_EXTRACT_DIR);
        system(cmd);

        printf("[UPDATE] Binary extracted successfully\n");
    }

    // Apply the update immediately
    update_apply();
    return true;
}

void update_apply(void) {
    const char *binary = get_binary_path();

    printf("[UPDATE] Applying update...\n");
    printf("[UPDATE] Target: %s\n", binary);

    // Create backup
    char backup_path[520];
    snprintf(backup_path, sizeof(backup_path), "%s%s", binary, BACKUP_SUFFIX);

    // Remove old backup if exists
    unlink(backup_path);

    // Backup current binary
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s' 2>/dev/null", binary, backup_path);
    if (system(cmd) != 0) {
        // Backup failed, but continue anyway (new install case)
        printf("[UPDATE] Backup failed (may be new install)\n");
    } else {
        printf("[UPDATE] Backup created: %s\n", backup_path);
    }

    // Replace binary
    snprintf(cmd, sizeof(cmd), "mv '%s' '%s'", TEMP_BINARY, binary);
    if (system(cmd) != 0) {
        snprintf(g_error, sizeof(g_error), "Failed to replace binary");
        // Try to restore backup
        snprintf(cmd, sizeof(cmd), "mv '%s' '%s' 2>/dev/null", backup_path, binary);
        system(cmd);
        g_state = UPDATE_ERROR;
        return;
    }

    // Make executable
    snprintf(cmd, sizeof(cmd), "chmod +x '%s'", binary);
    if (system(cmd) != 0) {
        printf("[UPDATE] Warning: chmod failed\n");
    }

    printf("[UPDATE] Update applied successfully!\n");
    g_state = UPDATE_READY;
}

UpdateState update_get_state(void) {
    return g_state;
}

const UpdateInfo* update_get_info(void) {
    return &g_info;
}

const char* update_get_error(void) {
    return g_error[0] ? g_error : NULL;
}

int update_get_progress(void) {
    return g_progress;
}

void update_reset(void) {
    g_state = UPDATE_IDLE;
    g_error[0] = '\0';
    g_progress = 0;
    g_is_zip = false;
    memset(&g_info, 0, sizeof(g_info));
}
