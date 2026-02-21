# Changelog

## [2026.02.21] - v1.9.3

### Fixed

- Fix(Launch): Auto-fix binary permissions on startup
  WHY: Pak Store and Windows SD card readers strip Unix execute permissions when extracting the zip. launch.sh now runs chmod +x on all binaries before executing, so Mono works regardless of how it was installed.

---

## [2026.02.20] - v1.9.2

### Fixed

- Fix(Update): Self-update now handles zip release assets
  WHY: Updater only looked for a bare "mono" binary in GitHub release assets, but v1.9.0+ releases use zip files (Mono.pak.zip, mono-release.zip). Now downloads zip, extracts bin/mono, and applies update. Falls back to bare binary for older releases.

- Fix(PakStore): Corrected release_filename in pak.json
  WHY: pak.json declared "Mono.pak.zip" but v1.9.1 release asset was "mono-release.zip". Pak Store couldn't find the asset, causing "Download Failed".

- Fix(Release): Include bare binary in release for backward compatibility
  WHY: Users on v1.9.0 or older have updaters that look for a bare "mono" asset. Including it alongside the zip ensures they can self-update without manual installation.

---

## [2026.02.14] - v1.9.1

### Fixed

- Fix(Build): Flatten release zip structure for Pak Store compatibility
  WHY: Zip contained a wrapper directory (Mono.pak/) instead of flat contents, causing Pak Store installation to fail.

---

## [2026.02.06] - v1.9.0

### Added

- Feat(UI): WiFi/Bluetooth status indicators in status bar
  WHY: Users need to know connectivity state at a glance for Spotify Connect and Bluetooth audio.

- Feat(UI): Dynamic list row calculation based on screen height
  WHY: Hardcoded row count didn't adapt to different screen resolutions or UI changes.

- Feat(UI): Scrolling text for long filenames in browser
  WHY: Long filenames were truncated — scrolling reveals the full name on highlight.

- Feat(UI): File extension dimming (gray .mp3/.flac suffixes)
  WHY: Reduces visual noise — the extension is secondary info compared to the track name.

### Changed

- Changed(Theme): Accent color changed to neon green (#33FF33)
  WHY: Higher contrast on the retro UI, better visibility on the Trimui screen.

- Changed(Sysinfo): WiFi/BT connectivity detection with 5-second cache
  WHY: Avoids expensive filesystem checks on every frame while staying responsive.

- Changed(UI): Resume playback dialog — responsive box sized to content
  WHY: Fixed-size box clipped text on some screens; content-driven sizing adapts to any resolution.

- Changed(UI): Consistent title-to-content spacing across all dialog screens
  WHY: Text was cramped on update, searching, and scanning screens after previous gap changes.

- Changed(UI): Status bar layout — better spacing between Vol, W, B, and battery
  WHY: W and B indicators were too close together and overlapping with volume text.

### Fixed

- Fix(UI): Help overlay now documents W (WiFi) and B (Bluetooth) status indicators
  WHY: Users couldn't tell what the W/B letters meant in the status bar.

---

## [2026.02.06] - v1.8.0

### Added

- Feat(Bluetooth): Bluetooth audio via bluealsa with A2DP
  WHY: Wireless headphone/speaker support — the most requested feature for a portable player.

- Feat(Spotify): Spotify Connect via librespot integration
  WHY: Stream from Spotify to the Trimui Brick, turning it into a Spotify Connect receiver.

- Feat(Update): Self-update from GitHub releases
  WHY: OTA updates without needing to manually copy files to SD card.

- Feat(Power): Pocket mode — power button suspend with LED heartbeat
  WHY: Screen off during pocket playback saves battery while confirming the device is alive.

- Feat(Input): L2/R2 analog trigger seek controls (±30s/±60s)
  WHY: Faster seeking through long tracks and podcasts.

- Feat(Audio): Audio format display (MP3/FLAC/OGG) in player
  WHY: Users want to know what format they're listening to.

### Changed

- Changed(Power): EVIOCGRAB exclusive grab on power button device
  WHY: Prevents keymon.elf from consuming power button events before Mono reads them.

- Changed(Launch): WiFi reconnect (ensure_wifi) in launch.sh
  WHY: NextUI may drop WiFi — reconnecting ensures Spotify/YouTube features work.

### Technical

- Added `spotify.c/h` — librespot lifecycle management
- Added `spotify_audio.c/h` — Spotify audio playback pipeline
- Added `spsearch.c/h` — Spotify search UI
- Added `update.c/h` — GitHub release checker and OTA updater
- Added `download_queue.c/h` — Background download manager
- Added `version.h` — Version string for update checks

---

## [2026.02.06] - v1.7.0

### Added

- Feat(Home): Home menu with Resume, Browse, Favorites, YouTube, Spotify
  WHY: Better navigation structure — dedicated entry points instead of cramming everything into the browser.

- Feat(EQ): 5-band parametric equalizer (60Hz, 250Hz, 1kHz, 4kHz, 16kHz)
  WHY: Audio customization for different headphones and genres, ±12dB per band.

- Feat(Audio): Gapless playback for FLAC via background preloading
  WHY: Eliminates the 200-600ms silence gap between tracks for seamless album listening.

- Feat(UI): Resume prompt when reopening tracks with saved positions
  WHY: Prevents accidental position loss — user chooses Resume or Start Over.

- Feat(Power): Power button suspend/resume support on Trimui
  WHY: Hardware power button integration for proper sleep/wake behavior.

- Feat(UI): Player title/artist text scrolling, toast notifications
  WHY: Long metadata was truncated — scrolling reveals full text.

### Technical

- Added `equalizer.c/h` — 5-band biquad IIR filter via Mix_SetPostMix
- Added `preload.c/h` — Background audio preloader for gapless playback

---

## [2026.02.05] - v1.6.0

### Added

- Feat(Cover): Album cover art display in player view
  WHY: Visual enhancement - shows cover.png/jpg, folder._, album._, front.\* from track directory.

- Feat(Theme): Dark/Light theme system with toggle
  WHY: User preference for different lighting conditions and battery optimization.

- Feat(YouTube): YouTube Music integration via yt-dlp
  WHY: Stream and download music directly from YouTube without leaving the app.

- Feat(Search): On-screen keyboard for YouTube search queries
  WHY: No physical keyboard on device - character picker UI enables text input.

- Feat(Metadata): MusicBrainz API metadata scanner
  WHY: Auto-populate missing ID3 tags by fingerprinting audio files.

- Feat(Positions): Per-file position persistence
  WHY: Resume playback exactly where you left off, even across different files.

- Feat(FileMenu): File management context menu (X button)
  WHY: Delete, rename files, and trigger metadata scans without leaving the app.

- Feat(FLAC): Native FLAC decoding via dr_flac
  WHY: Better FLAC support without SDL_mixer dependency issues.

- Feat(SysInfo): System information display
  WHY: Debug info and device stats for troubleshooting.

### Changed

- Refactor(UI): Implement 8-bit retro redesign with high contrast colors and blocky style
  WHY: To provide a distinct visual style and improve readability on low-res screens.

- Changed(UI): Disabled font anti-aliasing (TTF_HINTING_NONE)
  WHY: Simulates pixel font look and feels more retro.

- Changed(UI): Added thick borders to selection boxes and menus
  WHY: Enhances the 8-bit aesthetic and improves visual hierarchy.

### Technical

- Added `cover.c/h` - Album cover loading with stb_image
- Added `theme.c/h` - Theme management system
- Added `youtube.c/h` - yt-dlp integration
- Added `ytsearch.c/h` - YouTube search UI state
- Added `metadata.c/h` - MusicBrainz API client
- Added `positions.c/h` - Per-file position storage
- Added `filemenu.c/h` - File context menu
- Added `sysinfo.c/h` - System information
- Added `cJSON.c/h` - JSON parsing library
- Added `stb_image.h` - Image loading library
- Added `dr_flac.h` - FLAC decoding library

## [2026.02.04] - v1.5.0

### Added

- Feat(Screen): Screen dimming with SELECT button in player mode
  WHY: Battery saving during long playback sessions without turning off display completely.

- Feat(UI): Help overlay with Y button hold (300ms)
  WHY: Users need quick reference for controls without leaving the app.

- Feat(Launcher): Power-optimized launch.sh with CPU governor control
  WHY: Audio workload doesn't need full CPU speed, saves battery at 1.2GHz.

- Feat(Metadata): pak.json for NextUI app registry
  WHY: Standard metadata format for app stores and launchers.

### Fixed

- Fix(Audio): MP3 duration now calculated from frame header when Mix_MusicDuration unavailable
  WHY: Older SDL_mixer versions don't have Mix_MusicDuration, was showing "--:--".

- Fix(Display): Auto-detect screen dimensions with SDL_GetCurrentDisplayMode
  WHY: Hardcoded 1280x720 was causing overflow on some displays.

### Technical

- Added `screen.c/h` - Backlight control via sysfs interface
- Added `input_poll_holds()` for button hold detection
- Added `ui_render_help_browser()` and `ui_render_help_player()` overlays
- Added `estimate_mp3_duration()` fallback for MP3 duration calculation
- Launcher now sets CPU to 1.2GHz userspace governor with cleanup trap

---

## [2026.02.04] - v1.4.0

### Added

- Feat(State): Resume playback - app remembers last played track, position, and settings
  WHY: Users want to continue where they left off when reopening the app.

- Feat(Favorites): Mark tracks as favorites with Y button (shows \* indicator)
  WHY: Users need a way to quickly access their preferred tracks.

- Feat(State): Persist user preferences (volume, shuffle, repeat mode)
  WHY: Settings should survive app restarts without manual reconfiguration.

### Changed

- Changed(Input): Y button now toggles favorite (was shuffle, moved to Select)
  WHY: Dedicated button for favorites improves UX.

- Changed(UI): Browser shows \* prefix for favorite tracks in accent color
  WHY: Visual feedback helps users identify their favorites at a glance.

- Changed(UI): Player header shows \* when current track is a favorite
  WHY: Consistent favorite indicator across all views.

### Technical

- Added `state.c/h` - JSON-based state persistence (~/.mono/ on macOS, ~/.userdata/tg5040/Mono/ on device)
- Added `favorites.c/h` - Favorites management with JSON storage
- Added `browser_set_cursor()` and `browser_navigate_to()` for state restoration
- Added `menu_set_shuffle()` and `menu_set_repeat()` for preference restoration

---

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
