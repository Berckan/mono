# Mono

<!-- TODO: Add monito isotipo here -->

Feature-rich music player for **Trimui Brick** running NextUI/MinUI custom firmware.

![Mono Screenshot](docs/screenshot.png)

## Features

### Core Playback

- **Browse** `/Music/` folder on SD card
- **Play** MP3, FLAC, OGG, and WAV files
- **Display** ID3 metadata (title, artist, album)
- **Cover Art** - Shows album covers from folder
- **Resume** - Remembers position per file
- **Gapless Playback** - Seamless transitions between FLAC tracks

### Audio

- **5-Band Equalizer** - 60Hz, 250Hz, 1kHz, 4kHz, 16kHz (±12dB)
- **Bluetooth Audio** - A2DP wireless via bluealsa
- **Spotify Connect** - Use Trimui Brick as a Spotify receiver via librespot

### YouTube Integration

- **Search** YouTube Music with on-screen keyboard
- **Download** tracks via yt-dlp
- **Stream** directly without leaving the app

### Navigation

- **Home Menu** - Resume, Browse, Favorites, YouTube, Spotify
- **Resume List** - All tracks with saved positions
- **Favorites** - Quick access to starred tracks

### File Management

- **Rename** files with on-screen keyboard
- **Delete** files with confirmation
- **Scan** folders for metadata via MusicBrainz

### Customization

- **Themes** - Dark/Light mode toggle
- **8-bit UI** - Retro high-contrast design with neon green accents
- **Shuffle/Repeat** - Playback modes
- **Sleep Timer** - Auto-stop after 15/30/60 min

### System

- **Self-Update** - Check and install updates from GitHub releases
- **Pocket Mode** - Power button suspend with LED heartbeat
- **WiFi/Bluetooth Indicators** - Status bar shows connectivity
- **Screen Dimming** - Battery-saving display control

## Build

### Desktop (for testing)

```bash
# Requires SDL2, SDL2_mixer, SDL2_ttf
make desktop
./build/mono /path/to/music
```

### Trimui Brick

```bash
# Via Docker (recommended)
make docker

# Or with local toolchain
make tg5040
```

### Install

Copy `Mono.pak/` to `/Tools/tg5040/` on your SD card.

## Controls

### Home Menu

| Button   | Action       |
| -------- | ------------ |
| D-Pad ↑↓ | Navigate     |
| A        | Select       |
| B        | Exit app     |
| X        | Help overlay |
| Start    | Options menu |

### Browser Mode

| Button   | Action                    |
| -------- | ------------------------- |
| D-Pad ↑↓ | Navigate files            |
| A        | Select / Enter folder     |
| B        | Back / Parent folder      |
| X        | Help overlay              |
| Y        | Toggle favorite           |
| Select   | File menu (rename/delete) |
| Start    | Options menu              |
| Power    | Suspend (pocket mode)     |

### Player Mode

| Button   | Action                     |
| -------- | -------------------------- |
| D-Pad ↑↓ | Volume ±5%                 |
| D-Pad ←→ | Seek (accelerates on hold) |
| A        | Play / Pause               |
| B        | Back to browser            |
| X        | Help overlay               |
| Y        | Toggle favorite            |
| L/R      | Prev / Next track          |
| L2       | Jump to start of track     |
| R2       | Jump near end of track     |
| Start    | Options menu               |
| Select   | Dim screen                 |
| Power    | Suspend (pocket mode)      |
| Start+B  | Exit app                   |

### YouTube Search / Rename Keyboard

| Button | Action           |
| ------ | ---------------- |
| D-Pad  | Move cursor      |
| A      | Type character   |
| Y / B  | Backspace        |
| Start  | Confirm / Search |
| Select | Cancel           |

## Project Structure

```
mono/
├── src/
│   ├── main.c            # Entry point, state machine
│   ├── audio.c           # SDL_mixer playback
│   ├── browser.c         # File navigation
│   ├── ui.c              # SDL2 rendering
│   ├── input.c           # Button/power handling
│   ├── cover.c           # Album art loading
│   ├── theme.c           # Dark/Light themes
│   ├── equalizer.c       # 5-band parametric EQ
│   ├── preload.c         # Gapless playback preloader
│   ├── youtube.c         # yt-dlp integration
│   ├── ytsearch.c        # YouTube search UI
│   ├── spotify.c         # librespot lifecycle
│   ├── spotify_audio.c   # Spotify playback pipeline
│   ├── spsearch.c        # Spotify search UI
│   ├── metadata.c        # MusicBrainz API
│   ├── positions.c       # Position persistence
│   ├── filemenu.c        # File context menu
│   ├── state.c           # App state persistence
│   ├── favorites.c       # Favorites management
│   ├── screen.c          # Backlight/suspend control
│   ├── sysinfo.c         # System info (WiFi/BT)
│   ├── update.c          # OTA self-update
│   ├── download_queue.c  # Background downloads
│   ├── menu.c            # Options menu
│   └── version.h         # Version string
├── Mono.pak/             # Packaged app
│   ├── launch.sh         # Entry script
│   ├── bin/              # Compiled binary
│   └── pak.json          # NextUI metadata
├── Makefile
└── README.md
```

## Dependencies

- SDL2
- SDL2_mixer (MP3/FLAC/OGG support)
- SDL2_ttf (font rendering)
- yt-dlp (optional, for YouTube)
- curl (optional, for MusicBrainz/updates)
- librespot (optional, for Spotify Connect)
- bluealsa (optional, for Bluetooth audio)

## Known Issues

| Issue                                                                                  | Status       | Workaround                                                                         |
| -------------------------------------------------------------------------------------- | ------------ | ---------------------------------------------------------------------------------- |
| **FLAC Resume** — Some FLAC files restart from the beginning instead of saved position | Open         | Affects files without seek tables. MP3/OGG not affected                            |
| **Spotify Search** — Disabled, shows "Soon" in home menu                               | Blocked      | Spotify blocked new developer app creation (Feb 2026). Connect receiver works fine |
| **Bluetooth pairing** — Must be paired via NextUI settings before launching Mono       | By design    | Pair once in NextUI WiFi/BT settings, Mono auto-connects on launch                 |
| **Power button race** — Rare: power button unresponsive after wake from suspend        | Intermittent | Press power again, EVIOCGRAB re-grabs on next cycle                                |
| **Large libraries** — Initial scan of 1000+ files can be slow                          | Open         | Navigate into subfolders for faster loading                                        |

## Contributing

Found a bug? Want a feature? PRs welcome!

- [Report a bug](https://github.com/berckan/mono/issues/new?template=bug_report.yml)
- [Request a feature](https://github.com/berckan/mono/issues/new?template=feature_request.yml)
- [Contributing guide](CONTRIBUTING.md)

## License

MIT License - Berckan Guerrero

## Links

- [Trimui Brick](https://trimui.com)
- [NextUI Firmware](https://github.com/frfrwx/NextUI)
