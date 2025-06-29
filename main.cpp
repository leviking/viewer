#define RAYGUI_IMPLEMENTATION
#include "raylib.h"
#define PFD_IMPLEMENTATION
#include "portable-file-dialogs.h"

#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define MAX_IMAGES 256
#define THUMBNAIL_SIZE 128
#define PADDING 16

typedef struct {
    Texture2D texture;
    char path[512];
} ImageEntry;

bool HasImageExtension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".png") == 0 ||
            strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0);
}

int main(void) {
    InitWindow(1000, 800, "Raylib Image Viewer");
    SetTargetFPS(60);

    ImageEntry images[MAX_IMAGES];
    int imageCount = 0;

    // Pick folder
    auto folder = pfd::select_folder("Select a folder of images").result();
    if (folder.empty()) {
        CloseWindow();
        return 0;
    }

    // Load images from folder
    DIR *dir = opendir(folder.c_str());
    if (!dir) {
        CloseWindow();
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) && imageCount < MAX_IMAGES) {
        if (!HasImageExtension(entry->d_name)) continue;

        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", folder.c_str(), entry->d_name);

        Image img = LoadImage(fullPath);
        ImageResize(&img, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
        images[imageCount].texture = LoadTextureFromImage(img);
        strncpy(images[imageCount].path, entry->d_name, sizeof(images[imageCount].path));
        UnloadImage(img);
        imageCount++;
    }
    closedir(dir);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText(TextFormat("Loaded %d images from %s", imageCount, folder.c_str()), 10, 10, 20, DARKGRAY);

        // Layout thumbnails in grid
        int cols = GetScreenWidth() / (THUMBNAIL_SIZE + PADDING);
        for (int i = 0; i < imageCount; i++) {
            int x = (i % cols) * (THUMBNAIL_SIZE + PADDING) + PADDING;
            int y = (i / cols) * (THUMBNAIL_SIZE + PADDING) + 50;

            DrawTexture(images[i].texture, x, y, WHITE);
            DrawText(images[i].path, x, y + THUMBNAIL_SIZE + 4, 10, GRAY);
        }

        EndDrawing();
    }

    for (int i = 0; i < imageCount; i++) {
        UnloadTexture(images[i].texture);
    }

    CloseWindow();
    return 0;
}

