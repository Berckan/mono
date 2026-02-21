#!/bin/sh
# Mono - Minimalist Music Player
# Power-optimized launcher for Trimui Brick

APP_DIR="$(dirname "$0")"
APP_NAME="Mono"
LOG_DIR="${LOGS_PATH:-/tmp}"
LOG_FILE="$LOG_DIR/$APP_NAME.log"

cd "$APP_DIR" || exit 1

# Ensure binaries are executable (Pak Store may strip permissions)
chmod +x launch.sh bin/* 2>/dev/null

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

# Ensure WiFi is connected (NextUI kills WiFi when exiting apps)
ensure_wifi() {
    # Check if wlan0 has an IP
    if ! ip addr show wlan0 2>/dev/null | grep -q "inet "; then
        log "WiFi down, reconnecting..."
        rfkill.elf unblock wifi 2>/dev/null
        sleep 0.5
        /etc/init.d/wpa_supplicant start 2>/dev/null
        wpa_cli -i wlan0 reconnect 2>/dev/null
        # Wait up to 5s for connection
        for i in 1 2 3 4 5; do
            if ip addr show wlan0 2>/dev/null | grep -q "inet "; then
                log "WiFi reconnected"
                break
            fi
            sleep 1
        done
    else
        log "WiFi already connected"
    fi
}
ensure_wifi

export LD_LIBRARY_PATH="/usr/trimui/lib:$APP_DIR:$LD_LIBRARY_PATH"

# Audio routing - only use BT config if BT is actually connected
# Otherwise it breaks audio initialization
ASOUNDRC_SRC="/mnt/SDCARD/.userdata/tg5040/.asoundrc"
ASOUNDRC_DST="/root/.asoundrc"
# Check if bluetooth audio is active (bluealsa running with connected device)
if pgrep -x bluealsa > /dev/null && [ -f "$ASOUNDRC_SRC" ]; then
    if [ ! -f "$ASOUNDRC_DST" ] || [ "$ASOUNDRC_SRC" -nt "$ASOUNDRC_DST" ]; then
        cp "$ASOUNDRC_SRC" "$ASOUNDRC_DST" 2>/dev/null
        log "Using Bluetooth audio config"
    fi
else
    # No BT connected - remove config to use default audio
    rm -f "$ASOUNDRC_DST" 2>/dev/null
    log "Using default audio output"
fi

./bin/mono /mnt/SDCARD/Music 2>&1 | while IFS= read -r line; do
    echo "[$(date '+%H:%M:%S')] $line" >> "$LOG_FILE"
done
