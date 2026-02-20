#!/bin/sh
# validate-release.sh — Pre-release validation for Mono
# Checks version sync between version.h, pak.json, and CHANGELOG.md
# Usage: scripts/validate-release.sh [--pushing]
# --pushing: skip tag existence check (tag already created locally before push)
# Exit code 0 = all checks pass, non-zero = release blocked

set -e

PUSHING=false
for arg in "$@"; do
    case "$arg" in
        --pushing) PUSHING=true ;;
    esac
done

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

ERRORS=0
WARNINGS=0

pass() { printf "${GREEN}  PASS${NC} %s\n" "$1"; }
fail() { printf "${RED}  FAIL${NC} %s\n" "$1"; ERRORS=$((ERRORS + 1)); }
warn() { printf "${YELLOW}  WARN${NC} %s\n" "$1"; WARNINGS=$((WARNINGS + 1)); }

echo "=== Mono Release Validation ==="
echo ""

# --- 1. Extract versions ---

# version.h: VERSION string
VH_VERSION=$(sed -n 's/^#define VERSION "\(.*\)"/\1/p' src/version.h)
VH_MAJOR=$(sed -n 's/^#define VERSION_MAJOR \(.*\)/\1/p' src/version.h)
VH_MINOR=$(sed -n 's/^#define VERSION_MINOR \(.*\)/\1/p' src/version.h)
VH_PATCH=$(sed -n 's/^#define VERSION_PATCH \(.*\)/\1/p' src/version.h)

# pak.json: version (strip leading "v")
PAK_VERSION=$(python3 -c "import json; print(json.load(open('pak.json'))['version'].lstrip('v'))")
PAK_RELEASE_FN=$(python3 -c "import json; print(json.load(open('pak.json'))['release_filename'])")
PAK_CHANGELOG_KEYS=$(python3 -c "import json; print(' '.join(json.load(open('pak.json'))['changelog'].keys()))")

# --- 2. Version checks ---

# version.h VERSION matches pak.json version
if [ "$VH_VERSION" = "$PAK_VERSION" ]; then
    pass "version.h ($VH_VERSION) matches pak.json ($PAK_VERSION)"
else
    fail "version.h ($VH_VERSION) != pak.json ($PAK_VERSION)"
fi

# VERSION_MAJOR.MINOR.PATCH matches VERSION string
VH_COMPOSED="${VH_MAJOR}.${VH_MINOR}.${VH_PATCH}"
if [ "$VH_COMPOSED" = "$VH_VERSION" ]; then
    pass "VERSION_MAJOR.MINOR.PATCH ($VH_COMPOSED) matches VERSION ($VH_VERSION)"
else
    fail "VERSION_MAJOR.MINOR.PATCH ($VH_COMPOSED) != VERSION ($VH_VERSION)"
fi

# --- 3. release_filename check ---

if [ "$PAK_RELEASE_FN" = "mono-release.zip" ]; then
    pass "release_filename = mono-release.zip"
else
    fail "release_filename = '$PAK_RELEASE_FN' (expected 'mono-release.zip')"
fi

# --- 4. CHANGELOG.md has entry for version ---

if grep -q "v${VH_VERSION}" CHANGELOG.md; then
    pass "CHANGELOG.md has entry for v${VH_VERSION}"
else
    fail "CHANGELOG.md missing entry for v${VH_VERSION}"
fi

# --- 5. pak.json changelog has version key ---

if echo "$PAK_CHANGELOG_KEYS" | grep -q "v${VH_VERSION}"; then
    pass "pak.json changelog has v${VH_VERSION} key"
else
    fail "pak.json changelog missing v${VH_VERSION} key"
fi

# --- 6. Git tag doesn't already exist (skip when pushing) ---

if [ "$PUSHING" = true ]; then
    pass "Git tag check skipped (--pushing)"
elif git tag -l "v${VH_VERSION}" | grep -q "v${VH_VERSION}"; then
    fail "Git tag v${VH_VERSION} already exists"
else
    pass "Git tag v${VH_VERSION} does not exist yet"
fi

# --- 7. Working tree clean (warning only) ---

if [ -z "$(git status --porcelain)" ]; then
    pass "Working tree is clean"
else
    warn "Working tree has uncommitted changes"
fi

# --- Summary ---

echo ""
echo "=== Results ==="
if [ $ERRORS -gt 0 ]; then
    printf "${RED}BLOCKED${NC}: %d error(s), %d warning(s)\n" $ERRORS $WARNINGS
    exit 1
else
    if [ $WARNINGS -gt 0 ]; then
        printf "${GREEN}PASSED${NC} with %d warning(s)\n" $WARNINGS
    else
        printf "${GREEN}ALL CHECKS PASSED${NC}\n"
    fi
    exit 0
fi
