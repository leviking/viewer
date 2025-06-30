# Makefile for raylib-viewer (cross-platform)

# Compiler
CC = gcc

# Get raylib flags using pkg-config
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

# Compiler and linker flags
CFLAGS = -I. $(RAYLIB_CFLAGS)
LDFLAGS = $(RAYLIB_LIBS) -lm

# Source files and objects
SRCS = main.c ui.c state.c settings.c pdfgen.c tinyfiledialogs.c
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = raylib-viewer

# Default target
all: $(TARGET)

# Rule to build the final executable
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Rule to build object files
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

# Clean rule
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
