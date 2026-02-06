/**
 * Input Handler Implementation
 *
 * Maps keyboard/gamepad inputs to abstract actions.
 * Supports both desktop testing (keyboard) and Trimui Brick hardware.
 *
 * Trimui Brick Button Mapping (from NextUI platform.h):
 * - JOY_A = 1 (south button - confirm/select)
 * - JOY_B = 0 (east button - captured by system for launcher)
 * - JOY_X = 3 (west button - back, since B is captured)
 * - JOY_Y = 2 (north button)
 * - JOY_L1 = 4, JOY_R1 = 5 (shoulder buttons)
 * - JOY_SELECT = 6, JOY_START = 7
 * - JOY_MENU = 8
 */

#include "input.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#endif

// Keyboard mappings for desktop testing
// These mirror the Trimui Brick controls
// Prefixed with KBRD_ to avoid conflicts with linux/input.h KEY_* defines
#define KBRD_UP      SDLK_UP
#define KBRD_DOWN    SDLK_DOWN
#define KBRD_LEFT    SDLK_LEFT
#define KBRD_RIGHT   SDLK_RIGHT
#define KBRD_A       SDLK_z       // A button (confirm)
#define KBRD_B       SDLK_x       // B button (back)
#define KBRD_Y       SDLK_f       // Y button (favorite) - F key on keyboard
#define KBRD_L       SDLK_a       // L1 shoulder
#define KBRD_R       SDLK_s       // R1 shoulder
#define KBRD_L2      SDLK_q       // L2 trigger - jump to start
#define KBRD_R2      SDLK_w       // R2 trigger - jump to end
#define KBRD_START   SDLK_RETURN  // Start
#define KBRD_SELECT  SDLK_RSHIFT  // Select
#define KBRD_POWER   SDLK_p       // Power button (suspend) - P key on keyboard

// Trimui Brick joystick button indices (from NextUI platform.h)
// These match the official NextUI mapping for tg5040 platform
#define JOY_A       1   // South button (confirm)
#define JOY_B       0   // East button (captured by system)
#define JOY_X       3   // West button (back)
#define JOY_Y       2   // North button
#define JOY_L1      4   // Left shoulder
#define JOY_R1      5   // Right shoulder
// L2/R2 are analog triggers on axes, not buttons
#define AXIS_L2     2   // Left trigger axis (ABSZ)
#define AXIS_R2     5   // Right trigger axis (RABSZ)
#define JOY_SELECT  6
#define JOY_START   7
#define JOY_MENU    8
#define JOY_VOL_UP  11  // Volume up
#define JOY_VOL_DOWN 12 // Volume down
#define JOY_POWER   10  // Power button (for suspend)

// Button state tracking for debouncing (gamepad doesn't filter repeats like keyboard)
static Uint8 g_button_state[16] = {0};

// Time-based cooldown for buttons that need it (prevents physical bounce)
static Uint32 g_button_cooldown[16] = {0};
#define BUTTON_COOLDOWN_MS 250  // Minimum ms between button presses

// Start button tracking for combos (Start+B = exit)
static bool g_start_held = false;
static bool g_start_combo_used = false;  // True if combo was triggered while Start held

// Hat (D-Pad) state tracking - only trigger on state CHANGE
static Uint8 g_hat_state = SDL_HAT_CENTERED;

// Left/Right hold tracking for accelerated seek
static Uint32 g_seek_start_time = 0;  // When Left/Right was first pressed
static int g_seek_direction = 0;       // -1 for left, +1 for right, 0 for none
static Uint32 g_last_seek_tick = 0;    // Last time we returned a seek amount

// L2/R2 trigger debouncing (fire once per press, not continuously)
static bool g_l2_triggered = false;
static bool g_r2_triggered = false;

// Power button device (Linux only - reads from /dev/input/event1)
//
// IMPORTANT: Power button handling on Trimui/NextUI is complex!
//
// Problem: NextUI's keymon.elf daemon also reads from /dev/input/event1
// and can consume events before we read them, causing intermittent failures.
//
// Solution: Use EVIOCGRAB ioctl to get exclusive access to the device.
// This prevents keymon from stealing our events.
//
// Caveat: The exclusive grab is LOST after system suspend/resume!
// We must re-grab in input_drain_power() which is called after wake.
//
// Debugging: Check competing readers with:
//   ls -la /proc/*/fd/* 2>/dev/null | grep event1
//
// See MEMORY.md for full documentation of this issue.
#ifdef __linux__
static int g_power_fd = -1;
static int g_volume_fd = -1;
#define POWER_BUTTON_DEVICE "/dev/input/event1"
#define KEY_POWER_CODE 116       // KEY_POWER from linux/input-event-codes.h
#define KEY_VOLUMEDOWN_CODE 114  // KEY_VOLUMEDOWN
#define KEY_VOLUMEUP_CODE 115    // KEY_VOLUMEUP

// Find AVRCP device for BT volume control (returns fd or -1)
static int find_avrcp_device(void) {
    char path[128], name[256];
    for (int i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "/sys/class/input/event%d/device/name", i);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fgets(name, sizeof(name), f)) {
            fclose(f);
            if (strstr(name, "AVRCP")) {
                snprintf(path, sizeof(path), "/dev/input/event%d", i);
                int fd = open(path, O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    printf("[INPUT] Found AVRCP device: %s (%s)\n", path, name);
                    return fd;
                }
            }
        } else {
            fclose(f);
        }
    }
    return -1;
}
#endif


InputAction input_handle_event(const SDL_Event *event) {
    switch (event->type) {
        case SDL_KEYDOWN:
            // Ignore key repeats
            if (event->key.repeat) return INPUT_NONE;

            printf("[KEY] sym=%d\n", event->key.keysym.sym);

            switch (event->key.keysym.sym) {
                case KBRD_UP:     return INPUT_UP;
                case KBRD_DOWN:   return INPUT_DOWN;
                case KBRD_LEFT:   return INPUT_LEFT;
                case KBRD_RIGHT:  return INPUT_RIGHT;
                case KBRD_A:      return INPUT_SELECT;
                case KBRD_B:      return INPUT_BACK;
                case KBRD_Y:      return INPUT_FAVORITE;
                case KBRD_L:      return INPUT_PREV;
                case KBRD_R:      return INPUT_NEXT;
                case KBRD_L2:     return INPUT_SEEK_START;
                case KBRD_R2:     return INPUT_SEEK_END;
                case KBRD_START:  return INPUT_MENU;
                case KBRD_SELECT: return INPUT_SHUFFLE;
                case KBRD_POWER:  return INPUT_SUSPEND;  // P key for suspend on desktop
                case SDLK_ESCAPE:return INPUT_BACK;
                case SDLK_h:     return INPUT_HELP;  // H key for help on desktop
                // Hardware volume keys (Trimui Brick)
                case SDLK_VOLUMEUP:   return INPUT_VOL_UP;
                case SDLK_VOLUMEDOWN: return INPUT_VOL_DOWN;
                default: break;
            }
            break;

        // Raw Joystick API - used for Trimui Brick (Game Controller mapping is incorrect)
        case SDL_JOYBUTTONDOWN: {
            int btn = event->jbutton.button;
            Uint32 now = SDL_GetTicks();

            // Debouncing: ignore if button already pressed
            if (btn < 16 && g_button_state[btn]) {
                return INPUT_NONE;
            }

            // Time-based cooldown for Start/Menu buttons (physical bounce prevention)
            if (btn < 16 && (btn == JOY_START || btn == JOY_MENU)) {
                if (now - g_button_cooldown[btn] < BUTTON_COOLDOWN_MS) {
                    return INPUT_NONE;  // Too soon, ignore
                }
                g_button_cooldown[btn] = now;
            }

            if (btn < 16) g_button_state[btn] = 1;

            printf("[BTN] button=%d\n", btn);
            fflush(stdout);

            // TRIMUI Brick button mapping (from NextUI platform.h)
            switch (btn) {
                case JOY_A:      return INPUT_SELECT;   // A - confirm/play-pause
                case JOY_B:
                    // Check for Start+B combo (exit)
                    if (g_start_held) {
                        g_start_combo_used = true;
                        return INPUT_EXIT;
                    }
                    return INPUT_BACK;     // B - back
                case JOY_X:      return INPUT_HELP;     // X - toggle help overlay
                case JOY_Y:
                    return INPUT_FAVORITE;  // Y - toggle favorite
                case JOY_L1:
                    return INPUT_PREV;     // L1 - previous track
                case JOY_R1:
                    return INPUT_NEXT;     // R1 - next track
                // L2/R2 handled via axis (analog triggers)
                case JOY_SELECT: return INPUT_SHUFFLE;  // Select - shuffle/dim toggle
                case JOY_START:
                    // Track Start press for combos
                    g_start_held = true;
                    g_start_combo_used = false;
                    return INPUT_NONE;  // Don't trigger menu yet, wait for release
                case JOY_MENU:   return INPUT_MENU;     // Menu button
                case JOY_VOL_UP:   return INPUT_VOL_UP;   // Hardware volume up
                case JOY_VOL_DOWN: return INPUT_VOL_DOWN; // Hardware volume down
                case JOY_POWER:    return INPUT_SUSPEND;  // Power button
                default: break;
            }
            break;
        }

        case SDL_JOYBUTTONUP: {
            int btn = event->jbutton.button;
            if (btn < 16) g_button_state[btn] = 0;

            // Start button release: trigger menu if no combo was used
            if (btn == JOY_START) {
                g_start_held = false;
                if (!g_start_combo_used) {
                    return INPUT_MENU;  // Start tap = menu
                }
                g_start_combo_used = false;
            }

            return INPUT_NONE;
        }

        case SDL_JOYHATMOTION: {
            Uint8 new_state = event->jhat.value;
            Uint32 now = SDL_GetTicks();

            // Only trigger on state CHANGE (debouncing)
            if (new_state == g_hat_state) {
                return INPUT_NONE;
            }

            Uint8 old_state = g_hat_state;
            g_hat_state = new_state;

            // Track Left/Right for seek hold detection
            if (new_state == SDL_HAT_LEFT) {
                g_seek_start_time = now;
                g_seek_direction = -1;
                g_last_seek_tick = 0;
            } else if (new_state == SDL_HAT_RIGHT) {
                g_seek_start_time = now;
                g_seek_direction = 1;
                g_last_seek_tick = 0;
            } else if (new_state == SDL_HAT_CENTERED ||
                       new_state == SDL_HAT_UP ||
                       new_state == SDL_HAT_DOWN) {
                // Reset seek tracking when not left/right
                g_seek_start_time = 0;
                g_seek_direction = 0;
            }

            // Only trigger when a direction is NEWLY pressed
            // (not when released or when already held)
            if (new_state == SDL_HAT_CENTERED) {
                return INPUT_NONE;  // Released, no action
            }

            printf("[HAT] value=%d (was %d)\n", new_state, old_state);
            fflush(stdout);

            switch (new_state) {
                case SDL_HAT_UP:    return INPUT_UP;
                case SDL_HAT_DOWN:  return INPUT_DOWN;
                case SDL_HAT_LEFT:  return INPUT_LEFT;
                case SDL_HAT_RIGHT: return INPUT_RIGHT;
                default: break;
            }
            break;
        }

        case SDL_JOYAXISMOTION: {
            Uint32 now = SDL_GetTicks();

            // L2/R2 triggers: lower threshold (4000) for quick taps
            // These are simple actions: tap = jump to start/end
            if (event->jaxis.axis == AXIS_L2) {
                if (event->jaxis.value > 4000 && !g_l2_triggered) {
                    g_l2_triggered = true;
                    printf("[AXIS] L2 triggered (value=%d)\n", event->jaxis.value);
                    return INPUT_SEEK_START;
                } else if (event->jaxis.value < 2000) {
                    g_l2_triggered = false;
                }
                break;
            }
            if (event->jaxis.axis == AXIS_R2) {
                if (event->jaxis.value > 4000 && !g_r2_triggered) {
                    g_r2_triggered = true;
                    printf("[AXIS] R2 triggered (value=%d)\n", event->jaxis.value);
                    return INPUT_SEEK_END;
                } else if (event->jaxis.value < 2000) {
                    g_r2_triggered = false;
                }
                break;
            }

            // Other axes: joystick movement (higher threshold)
            if (abs(event->jaxis.value) > 16000) {
                printf("[AXIS] axis=%d value=%d\n", event->jaxis.axis, event->jaxis.value);
                fflush(stdout);

                if (event->jaxis.axis == 0) {  // X axis (Left/Right)
                    if (event->jaxis.value < 0) {
                        // LEFT pressed via axis - track for seek
                        g_seek_start_time = now;
                        g_seek_direction = -1;
                        g_last_seek_tick = 0;
                        return INPUT_LEFT;
                    } else {
                        // RIGHT pressed via axis - track for seek
                        g_seek_start_time = now;
                        g_seek_direction = 1;
                        g_last_seek_tick = 0;
                        return INPUT_RIGHT;
                    }
                } else if (event->jaxis.axis == 1) {  // Y axis (Up/Down)
                    return (event->jaxis.value < 0) ? INPUT_UP : INPUT_DOWN;
                }
            } else if (abs(event->jaxis.value) < 8000) {
                // Axis returned to center - reset seek tracking
                if (event->jaxis.axis == 0 && g_seek_direction != 0) {
                    g_seek_start_time = 0;
                    g_seek_direction = 0;
                }
            }
            break;
        }

        default:
            break;
    }

    return INPUT_NONE;
}

InputAction input_poll_holds(void) {
    // No hold detection needed - X triggers help directly
    // Keep function for potential future use
    return INPUT_NONE;
}

bool input_is_seeking(void) {
    return g_seek_direction != 0 && g_seek_start_time > 0;
}

int input_get_seek_amount(void) {
    if (!input_is_seeking()) {
        return 0;
    }

    Uint32 now = SDL_GetTicks();
    Uint32 held_ms = now - g_seek_start_time;

    // Rate limit: only return seek amount every 150ms
    if (g_last_seek_tick > 0 && now - g_last_seek_tick < 150) {
        return 0;
    }
    g_last_seek_tick = now;

    // Acceleration based on hold duration
    // Optimized thresholds for faster response (was 1000/2000/3000ms)
    int seek_seconds;
    if (held_ms < 400) {
        // 0-400ms: 5 seconds per tick (fine seeking)
        seek_seconds = 5;
    } else if (held_ms < 1000) {
        // 400ms-1s: 15 seconds per tick
        seek_seconds = 15;
    } else if (held_ms < 2000) {
        // 1-2 seconds: 30 seconds per tick
        seek_seconds = 30;
    } else {
        // 2+ seconds: 60 seconds per tick (1 minute)
        seek_seconds = 60;
    }

    return seek_seconds * g_seek_direction;
}

bool input_init(void) {
#ifdef __linux__
    // Open power button device (non-blocking)
    g_power_fd = open(POWER_BUTTON_DEVICE, O_RDONLY | O_NONBLOCK);
    if (g_power_fd < 0) {
        printf("[INPUT] Warning: Could not open power button device %s\n", POWER_BUTTON_DEVICE);
        // Not fatal - power button just won't work
    } else {
        printf("[INPUT] Power button device opened: %s (fd=%d)\n", POWER_BUTTON_DEVICE, g_power_fd);

        // Grab device exclusively to prevent keymon.elf from consuming events
        if (ioctl(g_power_fd, EVIOCGRAB, 1) < 0) {
            printf("[INPUT] Warning: Could not grab power button exclusively: %s\n", strerror(errno));
        } else {
            printf("[INPUT] Power button grabbed exclusively\n");
        }
    }

    // Find and open AVRCP device for BT volume control
    g_volume_fd = find_avrcp_device();
    if (g_volume_fd >= 0) {
        // Grab device exclusively to handle volume ourselves
        if (ioctl(g_volume_fd, EVIOCGRAB, 1) < 0) {
            printf("[INPUT] Warning: Could not grab AVRCP volume exclusively: %s\n", strerror(errno));
        } else {
            printf("[INPUT] AVRCP volume grabbed exclusively\n");
        }
    } else {
        printf("[INPUT] No AVRCP device found (BT headphone not connected?)\n");
    }
#endif
    return true;
}

void input_cleanup(void) {
#ifdef __linux__
    if (g_power_fd >= 0) {
        // Release exclusive grab before closing
        ioctl(g_power_fd, EVIOCGRAB, 0);
        close(g_power_fd);
        g_power_fd = -1;
        printf("[INPUT] Power button device closed\n");
    }
    if (g_volume_fd >= 0) {
        ioctl(g_volume_fd, EVIOCGRAB, 0);
        close(g_volume_fd);
        g_volume_fd = -1;
        printf("[INPUT] AVRCP volume device closed\n");
    }
#endif
}

InputAction input_poll_power(void) {
#ifdef __linux__
    if (g_power_fd < 0) {
        return INPUT_NONE;
    }

    struct input_event ev;
    while (read(g_power_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        // Check for KEY_POWER press (type=EV_KEY, code=KEY_POWER, value=1)
        // value: 1=press, 0=release, 2=repeat (held down)
        if (ev.type == EV_KEY && ev.code == KEY_POWER_CODE && ev.value == 1) {
            printf("[POWER] Power button pressed\n");
            fflush(stdout);
            return INPUT_SUSPEND;
        }
    }
#endif
    return INPUT_NONE;
}

void input_drain_power(void) {
#ifdef __linux__
    if (g_power_fd < 0) {
        return;
    }

    // Small delay to let any pending events arrive
    usleep(100000);  // 100ms

    // Re-grab device after wake (grab may be lost after system suspend)
    if (ioctl(g_power_fd, EVIOCGRAB, 1) < 0) {
        printf("[POWER] Warning: Re-grab failed after wake: %s\n", strerror(errno));
    }

    // Drain all pending events
    struct input_event ev;
    int drained = 0;
    while (read(g_power_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        drained++;
    }
    printf("[POWER] Drained %d events after wake\n", drained);
    fflush(stdout);
#endif
}

InputAction input_poll_volume(void) {
#ifdef __linux__
    if (g_volume_fd < 0) {
        return INPUT_NONE;
    }

    struct input_event ev;
    while (read(g_volume_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        // Debug: log all key events from AVRCP
        if (ev.type == EV_KEY) {
            printf("[AVRCP] Key event: code=%d value=%d\n", ev.code, ev.value);
            fflush(stdout);
        }
        // Check for volume key press (type=EV_KEY, value=1 for press)
        if (ev.type == EV_KEY && ev.value == 1) {
            if (ev.code == KEY_VOLUMEUP_CODE) {
                printf("[AVRCP] Volume UP detected\n");
                return INPUT_VOL_UP;
            } else if (ev.code == KEY_VOLUMEDOWN_CODE) {
                printf("[AVRCP] Volume DOWN detected\n");
                return INPUT_VOL_DOWN;
            }
        }
    }
#endif
    return INPUT_NONE;
}
