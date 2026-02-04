# Changelog

## [2026.02.04] - v1.3.0

### Added

- Feat(Menu): Fully interactive options menu (Start button)
  - **Shuffle**: On/Off - play tracks in random order
  - **Repeat**: Off/One/All - repeat modes
  - **Sleep Timer**: Off/15/30/60 minutes - auto-stop playback
  - **Exit**: Return to browser and stop playback
    WHY: Users need control over playback behavior without exiting the player.

### Changed

- Changed(Audio): Auto-advance now respects Shuffle and Repeat settings
  WHY: Menu options need to affect playback behavior.

---

## [2026.02.04] - v1.2.4

### Fixed

- Fix(Audio): Pause no longer triggers auto-advance to next track
  WHY: `update()` checked `!audio_is_playing()` which is true when paused.
  Now also checks `!audio_is_paused()` to distinguish pause from track end.

---

## [2026.02.04] - v1.2.3

### Fixed

- Fix(Input): Use correct button indices from NextUI platform.h
  WHY: Was guessing button indices (A=0, X=1). Official NextUI mapping is:
  JOY_A=1, JOY_B=0, JOY_X=3, JOY_Y=2. Now matches other NextUI apps.

---

## [2026.02.04] - v1.2.2

### Fixed

- Fix(Input): Revert to raw SDL Joystick API (remove Game Controller API)
  WHY: SDL's gamecontrollerdb has incorrect mapping for "TRIMUI Player1".

---

## [2026.02.04] - v1.2.1

### Fixed

- Fix(Input): Attempted migration to SDL Game Controller API (reverted in v1.2.2)
  WHY: Theory was that raw joystick indices are device-specific, but in practice
  the Game Controller mapping was worse than raw indices for Trimui Brick.

---

## [2026.02.04] - v1.2.0

### Fixed

- Fix(Audio): Track duration now displays correctly using Mix_MusicDuration()
  WHY: Duration showed "0:00" because `duration_sec` was never initialized from SDL_mixer

- Fix(Input): Corrected A/X button mapping (A=0, X=1 on Trimui Brick hardware)
  WHY: Buttons were inverted - A was closing app instead of selecting

- Fix(Input): Added gamepad button debouncing to prevent Start menu flickering
  WHY: SDL_JOYBUTTONDOWN doesn't filter repeats like SDL_KEYDOWN does

- Fix(Input): D-Pad UP/DOWN now controls volume in player mode
  WHY: Hardware volume keys are captured by system before reaching SDL

- Fix(UI): Improved font rendering with TTF_HINTING_LIGHT for small LCD screens
  WHY: Default hinting produced pixelated text on 320x240 display

### Changed

- Changed(UI): Display "--:--" when track duration is unknown
  WHY: Better UX than showing misleading "0:00" when metadata unavailable

- Changed(UI): Applied SDL render hints before creating renderer
  WHY: Ensures linear filtering is applied to all textures

---

## [2026.02.03] - v1.1.0

### Added

- Initial release with file browser, audio playback, and minimalist UI
