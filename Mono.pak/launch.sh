#!/bin/sh
# Mono - Minimalist Music Player
# Power-optimized launcher for Trimui Brick

APP_DIR="$(dirname "$0")"
APP_NAME="Mono"
LOG_DIR="${LOGS_PATH:-/tmp}"
LOG_FILE="$LOG_DIR/$APP_NAME.log"

cd "$APP_DIR" || exit 1

# Rotate log if over 100KB
if [ -f "$LOG_FILE" ] && [ "$(stat -c%s "$LOG_FILE" 2>/dev/null || echo 0)" -gt 102400 ]; then
    mv "$LOG_FILE" "$LOG_FILE.old"
fi

log() {
    echo "[$(date '+%H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

log "Starting $APP_NAME"

# Save original CPU settings
ORIG_GOVERNOR=""
if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
    ORIG_GOVERNOR=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)
fi

# Audio workload doesn't need full CPU speed - 1.2GHz is sweet spot
# (1.0GHz can cause stutters, full speed wastes battery)
setup_power_profile() {
    if [ -w /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        echo userspace > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
        echo 1200000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2>/dev/null
        log "Power profile: 1.2GHz userspace"
    fi
}

restore_power_profile() {
    if [ -n "$ORIG_GOVERNOR" ] && [ -w /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        echo "$ORIG_GOVERNOR" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
        log "Power profile: restored $ORIG_GOVERNOR"
    fi
    log "Shutdown complete"
}

# Cleanup on exit (SIGTERM, SIGINT, normal exit)
trap restore_power_profile EXIT

setup_power_profile

export LD_LIBRARY_PATH="/usr/trimui/lib:$APP_DIR:$LD_LIBRARY_PATH"

./mono.elf /mnt/SDCARD/Music 2>&1 | while IFS= read -r line; do
    echo "[$(date '+%H:%M:%S')] $line" >> "$LOG_FILE"
done
