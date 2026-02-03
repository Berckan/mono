# Mono

Minimalist MP3 player for **Trimui Brick** running NextUI/MinUI custom firmware.

![Mono Screenshot](docs/screenshot.png)

## Features

- **Browse** `/Music/` folder on SD card
- **Play** MP3, FLAC, OGG, and WAV files
- **Display** ID3 metadata (title, artist)
- **Controls** via D-Pad and buttons
- **Dark theme** optimized for battery life

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

| Button   | Action              |
| -------- | ------------------- |
| D-Pad ↑↓ | Navigate            |
| D-Pad ←→ | Seek ±10s           |
| A        | Select / Play-Pause |
| B        | Back / Exit         |
| L/R      | Prev / Next track   |
| Start    | Menu                |

## Project Structure

```
mono/
├── src/
│   ├── main.c      # Entry point
│   ├── audio.c     # SDL_mixer playback
│   ├── browser.c   # File navigation
│   ├── ui.c        # SDL2 rendering
│   └── input.c     # Button handling
├── Mono.pak/       # Packaged app
│   ├── launch.sh   # Entry script
│   ├── bin/        # Compiled binary
│   └── assets/     # Fonts, icons
├── Makefile
└── README.md
```

## Dependencies

- SDL2
- SDL2_mixer (MP3/FLAC/OGG support)
- SDL2_ttf (font rendering)

## License

MIT License - Berckan Guerrero

## Links

- [Trimui Brick](https://trimui.com)
- [NextUI Firmware](https://github.com/frfrwx/NextUI)
- [SDLReader-brick](https://github.com/Helaas/SDLReader-brick) (reference)
