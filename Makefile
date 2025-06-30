# Makefile for raylib-viewer (cross-platform)

# Compiler
CC = gcc

# Build directory
BUILD_DIR = build
TARGET = $(BUILD_DIR)/raylib-viewer

# Get raylib flags using pkg-config
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

# Compiler and linker flags
CFLAGS = -I. $(RAYLIB_CFLAGS)
LDFLAGS = $(RAYLIB_LIBS) -lm

# Source files and objects
SRCS = main.c ui.c state.c settings.c pdfgen.c tinyfiledialogs.c
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Rule to build the final executable
$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(LDFLAGS)

# Rule to build object files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

# Clean rule
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)