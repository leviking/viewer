#include "raylib.h"
#define PFD_IMPLEMENTATION
#include "portable-file-dialogs.h"

#include <dirent.h>
#include <string.h>
#include <vector>
#include <string>

#define THUMBNAIL_SIZE 128
#define PADDING 16
#define MAX_IMAGES 4096

typedef struct {
    char path[512];
    Texture2D texture;
    bool loaded;
} ImageEntry;

bool HasImageExtension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".png") == 0 ||
            strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0);
}

int main(void) {
    InitWindow(1000, 800, "Raylib Lazy Image Viewer");
    SetTargetFPS(60);

    ImageEntry images[MAX_IMAGES];
    int imageCount = 0;

    // Select folder using portable-file-dialogs
    auto folder = pfd::select_folder("Select a folder of images").result();
    if (folder.empty()) {
        CloseWindow();
        return 0;
    }

    // Scan folder and store paths only
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

        strncpy(images[imageCount].path, fullPath, sizeof(images[imageCount].path));
        images[imageCount].loaded = false;
        imageCount++;
    }
    closedir(dir);

    float scrollY = 0;
    int cols = GetScreenWidth() / (THUMBNAIL_SIZE + PADDING);
    int rows = (imageCount + cols - 1) / cols;
    float maxScroll = rows * (THUMBNAIL_SIZE + PADDING);

    while (!WindowShouldClose()) {
        // Scroll with mouse wheel
        float wheel = GetMouseWheelMove();
        scrollY += wheel * 30;
        if (scrollY > 0) scrollY = 0;
        if (scrollY < GetScreenHeight() - maxScroll - 50)
            scrollY = GetScreenHeight() - maxScroll - 50;

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText(TextFormat("Loaded %d images from %s", imageCount, folder.c_str()), 10, 10, 20, DARKGRAY);

        cols = GetScreenWidth() / (THUMBNAIL_SIZE + PADDING);

        for (int i = 0; i < imageCount; i++) {
            int x = (i % cols) * (THUMBNAIL_SIZE + PADDING) + PADDING;
            int y = (i / cols) * (THUMBNAIL_SIZE + PADDING) + 50 + scrollY;

            // Only process if visible
            if (y + THUMBNAIL_SIZE < 0 || y > GetScreenHeight()) continue;

            // Lazy load
            if (!images[i].loaded) {
                Image img = LoadImage(images[i].path);
                if (img.data != NULL) {
                    ImageResize(&img, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
                    images[i].texture = LoadTextureFromImage(img);
                    UnloadImage(img);
                    images[i].loaded = true;
                }
            }

            if (images[i].loaded) {
                DrawTexture(images[i].texture, x, y, WHITE);
            }
        }

        EndDrawing();
    }

    // Unload only loaded textures
    for (int i = 0; i < imageCount; i++) {
        if (images[i].loaded) {
            UnloadTexture(images[i].texture);
        }
    }

    CloseWindow();
    return 0;
}

