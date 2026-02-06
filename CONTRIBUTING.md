# Contributing to Mono

Thanks for your interest in contributing!

## Reporting Bugs

[Open a bug report](https://github.com/berckan/mono/issues/new?template=bug_report.yml) with:

- Mono version and firmware (NextUI/MinUI)
- Steps to reproduce
- Logs from `/mnt/SDCARD/.userdata/tg5040/logs/Mono.log` if available

## Requesting Features

[Open a feature request](https://github.com/berckan/mono/issues/new?template=feature_request.yml) describing what you'd like and why it's useful.

## Pull Requests

1. Fork the repo
2. Make your changes
3. Test with `make docker` (ARM build) and optionally `make desktop` (macOS/Linux)
4. Open a PR with a clear description

### Build Requirements

**Desktop (testing):**

- SDL2, SDL2_mixer, SDL2_ttf
- `make desktop`

**Trimui Brick (production):**

- Docker
- `make docker`

### Code Style

- C99 with SDL2
- 4-space indentation
- Functions prefixed by module name (`ui_render_`, `audio_play_`, `browser_get_`)
- Keep functions short — extract helpers over 50 lines
- No dynamic allocation where stack/static suffices

### What Makes a Good PR

- Solves one thing well
- Compiles without warnings on `make docker`
- Doesn't break existing controls (test the help overlay)
- If adding a new control, update the help overlay text in `ui.c`
