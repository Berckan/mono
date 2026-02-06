# Mono

<!-- TODO: Add monito isotipo here -->

Minimalist MP3 player for **Trimui Brick** running NextUI/MinUI custom firmware.

![Mono Screenshot](docs/screenshot.png)

## Features

### Core Playback

- **Browse** `/Music/` folder on SD card
- **Play** MP3, FLAC, OGG, and WAV files
- **Display** ID3 metadata (title, artist, album)
- **Cover Art** - Shows album covers from folder
- **Resume** - Remembers position per file

### YouTube Integration

- **Search** YouTube Music with on-screen keyboard
- **Download** tracks via yt-dlp
- **Stream** directly without leaving the app

### File Management

- **Rename** files with on-screen keyboard
- **Delete** files with confirmation
- **Scan** folders for metadata via MusicBrainz

### Customization

- **Themes** - Dark/Light mode toggle
- **8-bit UI** - Retro high-contrast design
- **Shuffle/Repeat** - Playback modes
- **Sleep Timer** - Auto-stop after 15/30/60 min

## Build

### Desktop (for testing)

```bash
# Requires SDL2, SDL2_mixer, SDL2_ttf
make desktop
./build/mono
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

### Browser Mode

| Button   | Action                    |
| -------- | ------------------------- |
| D-Pad ↑↓ | Navigate files            |
| A        | Select / Enter            |
| B        | Back / Exit               |
| X        | File menu (rename/delete) |
| Y        | Toggle favorite           |
| Start    | YouTube search            |

### Player Mode

| Button   | Action            |
| -------- | ----------------- |
| D-Pad ↑↓ | Volume ±5%        |
| D-Pad ←→ | Seek ±10s         |
| A        | Play / Pause      |
| B        | Back to browser   |
| X        | Cycle theme       |
| Y        | Toggle favorite   |
| L/R      | Prev / Next track |
| Start    | Options menu      |
| Select   | Dim screen        |

## Project Structure

```
mono/
├── src/
│   ├── main.c        # Entry point, app loop
│   ├── audio.c       # SDL_mixer playback
│   ├── browser.c     # File navigation
│   ├── ui.c          # SDL2 rendering
│   ├── input.c       # Button handling
│   ├── cover.c       # Album art loading
│   ├── theme.c       # Dark/Light themes
│   ├── youtube.c     # yt-dlp integration
│   ├── ytsearch.c    # YouTube search UI
│   ├── metadata.c    # MusicBrainz API
│   ├── positions.c   # Position persistence
│   ├── filemenu.c    # File context menu
│   └── state.c       # App state persistence
├── Mono.pak/         # Packaged app
│   ├── launch.sh     # Entry script
│   ├── bin/          # Compiled binary
│   └── pak.json      # NextUI metadata
├── Makefile
└── README.md
```

## Dependencies

- SDL2
- SDL2_mixer (MP3/FLAC/OGG support)
- SDL2_ttf (font rendering)
- yt-dlp (optional, for YouTube)
- curl (optional, for MusicBrainz)

## Known Issues

- **FLAC Resume**: Some FLAC files may start from the beginning instead of the saved position when resuming. This affects files without proper seek tables. MP3 files are not affected.

## License

MIT License - Berckan Guerrero

## Links

- [Trimui Brick](https://trimui.com)
- [NextUI Firmware](https://github.com/frfrwx/NextUI)
