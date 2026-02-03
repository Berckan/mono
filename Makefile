# Mono - MP3 Player for Trimui Brick
# Makefile for cross-compilation

# Project settings
TARGET = mono
BUILD_DIR = build
SRC_DIR = src
PAK_DIR = Mono.pak

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Common flags
COMMON_CFLAGS = -Wall -Wextra -O2 -DSDL_MAIN_HANDLED
COMMON_LDFLAGS =

# SDL flags (common)
SDL_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_mixer SDL2_ttf 2>/dev/null || echo "-I/usr/include/SDL2")
SDL_LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_mixer SDL2_ttf 2>/dev/null || echo "-lSDL2 -lSDL2_mixer -lSDL2_ttf")

# Platform detection and configuration
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Default target is desktop
.DEFAULT_GOAL := desktop

# Desktop build (native)
ifeq ($(PLATFORM),)
    ifeq ($(UNAME_S),Darwin)
        # macOS
        CC = clang
        CFLAGS = $(COMMON_CFLAGS) $(SDL_CFLAGS)
        LDFLAGS = $(COMMON_LDFLAGS) $(SDL_LDFLAGS)
    else
        # Linux
        CC = gcc
        CFLAGS = $(COMMON_CFLAGS) $(SDL_CFLAGS)
        LDFLAGS = $(COMMON_LDFLAGS) $(SDL_LDFLAGS)
    endif
endif

# Trimui Brick (tg5040) cross-compilation
ifeq ($(PLATFORM),tg5040)
    TOOLCHAIN ?= /opt/tg5040-toolchain
    SYSROOT = $(TOOLCHAIN)/arm-buildroot-linux-gnueabihf/sysroot
    CC = $(TOOLCHAIN)/bin/arm-buildroot-linux-gnueabihf-gcc
    STRIP = $(TOOLCHAIN)/bin/arm-buildroot-linux-gnueabihf-strip

    CFLAGS = $(COMMON_CFLAGS) \
        --sysroot=$(SYSROOT) \
        -I$(SYSROOT)/usr/include/SDL2 \
        -I$(SYSROOT)/usr/include \
        -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard

    LDFLAGS = $(COMMON_LDFLAGS) \
        --sysroot=$(SYSROOT) \
        -L$(SYSROOT)/usr/lib \
        -Wl,-rpath-link,$(SYSROOT)/usr/lib \
        -lSDL2 -lSDL2_mixer -lSDL2_ttf
endif

# Build rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Desktop build
desktop: $(BUILD_DIR)/$(TARGET)
	@echo "Built $(TARGET) for desktop"

# Trimui Brick build
tg5040:
	$(MAKE) PLATFORM=tg5040 $(BUILD_DIR)/$(TARGET)
	$(STRIP) $(BUILD_DIR)/$(TARGET)
	@mkdir -p $(PAK_DIR)/bin
	cp $(BUILD_DIR)/$(TARGET) $(PAK_DIR)/bin/
	@echo "Built $(TARGET) for Trimui Brick"
	@echo "Copy $(PAK_DIR)/ to /Tools/tg5040/ on SD card"

# Docker build (for those without local toolchain)
docker:
	docker run --rm -v $(PWD):/src -w /src \
		ghcr.io/loveretro/tg5040-toolchain:latest \
		make PLATFORM=tg5040 TOOLCHAIN=/opt/toolchain $(BUILD_DIR)/$(TARGET)
	@mkdir -p $(PAK_DIR)/bin
	cp $(BUILD_DIR)/$(TARGET) $(PAK_DIR)/bin/
	@echo "Built $(TARGET) via Docker for Trimui Brick"

# Clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(PAK_DIR)/bin/$(TARGET)

# Install to SD card (macOS - adjust for your mount point)
install: tg5040
	@if [ -d "/Volumes/SDCARD/Tools/tg5040" ]; then \
		cp -r $(PAK_DIR) "/Volumes/SDCARD/Tools/tg5040/"; \
		echo "Installed to SD card"; \
	else \
		echo "SD card not mounted at /Volumes/SDCARD"; \
	fi

# Package release
release: tg5040
	@rm -f $(TARGET)-release.zip
	zip -r $(TARGET)-release.zip $(PAK_DIR)
	@echo "Created $(TARGET)-release.zip"

# Debug info
info:
	@echo "Platform: $(UNAME_S) $(UNAME_M)"
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"

.PHONY: desktop tg5040 docker clean install release info
