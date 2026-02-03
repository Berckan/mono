#!/bin/sh
# Mono - MP3 Player for Trimui Brick
# Launch script for NextUI/MinUI

# Get the directory where this script is located
DIR="$(dirname "$0")"

# Default music path (can be overridden)
MUSIC_PATH="/mnt/SDCARD/Music"

# Check if Music folder exists, fallback to root
if [ ! -d "$MUSIC_PATH" ]; then
    MUSIC_PATH="/mnt/SDCARD"
fi

# Hide cursor (if applicable)
echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true

# Run the player
exec "$DIR/bin/mono" "$MUSIC_PATH"
