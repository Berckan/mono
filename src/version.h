/**
 * Version Information
 *
 * Centralized version definition for the Mono application.
 * Keep in sync with Mono.pak/pak.json when releasing.
 */

#ifndef VERSION_H
#define VERSION_H

// Version string (displayed to user)
#define VERSION "1.9.0"

// Version components for comparison
#define VERSION_MAJOR 1
#define VERSION_MINOR 9
#define VERSION_PATCH 0

// User agent for API requests
#define VERSION_USER_AGENT "Mono/" VERSION " (https://github.com/berckan/mono)"

// GitHub repository for update checks
#define GITHUB_REPO_OWNER "berckan"
#define GITHUB_REPO_NAME "mono"

#endif // VERSION_H
