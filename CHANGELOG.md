# Changelog

## [2026.02.04] - v1.2.2

### Fixed

- Fix(Input): Revert to raw SDL Joystick API (remove Game Controller API)
  WHY: SDL's gamecontrollerdb has incorrect mapping for "TRIMUI Player1" - buttons
  were shifted (A→Start, B→A, X→B). Raw joystick indices are correct for this device.

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
