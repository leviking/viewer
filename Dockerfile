# Use a stable base image with build tools
FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install all necessary dependencies for Linux and Windows cross-compilation
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    pkg-config \
    git \
    cmake \
    mingw-w64 \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev

# --- Build raylib from source ---
WORKDIR /libs
RUN git clone https://github.com/raysan5/raylib.git
WORKDIR /libs/raylib/src
# Build for Linux
RUN make PLATFORM=PLATFORM_DESKTOP
# Build for Windows
RUN make PLATFORM=PLATFORM_DESKTOP CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar

# Copy your application source code
WORKDIR /app
COPY . .

# --- Build your application ---
# Use g++ because main.c now contains C++ code for portable-file-dialogs
# Build for Linux
RUN g++ main.c pdfgen.c -o viewer-linux -I/libs/raylib/src -L/libs/raylib/src -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# Build for Windows
RUN x86_64-w64-mingw32-g++ main.c pdfgen.c -o viewer-windows.exe -I/libs/raylib/src -L/libs/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm -static

# Final stage to hold the artifacts
FROM scratch
COPY --from=builder /app/viewer-linux /
COPY --from=builder /app/viewer-windows.exe /