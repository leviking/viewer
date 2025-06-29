#include "raylib.h"
#define PFD_IMPLEMENTATION
#include "portable-file-dialogs.h"

#include <dirent.h>
#include <string.h>

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

    // Folder dialog
    auto folder = pfd::select_folder("Select a folder of images").result();
    if (folder.empty()) {
        CloseWindow();
        return 0;
    }

    // Scan folder
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
        // Scroll input
        float wheel = GetMouseWheelMove();
        scrollY += wheel * 30;
        if (scrollY > 0) scrollY = 0;
        if (scrollY < GetScreenHeight() - maxScroll - 50)
            scrollY = GetScreenHeight() - maxScroll - 50;

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText(TextFormat("Loaded %d / %d images from %s", 
            [&]() {
                int n = 0;
                for (int i = 0; i < imageCount; i++) if (images[i].loaded) n++;
                return n;
            }(), imageCount, folder.c_str()), 10, 10, 20, DARKGRAY);

        bool loadedOneThisFrame = false;
        cols = GetScreenWidth() / (THUMBNAIL_SIZE + PADDING);

        for (int i = 0; i < imageCount; i++) {
            int x = (i % cols) * (THUMBNAIL_SIZE + PADDING) + PADDING;
            int y = (i / cols) * (THUMBNAIL_SIZE + PADDING) + 50 + scrollY;

            if (y + THUMBNAIL_SIZE < 0 || y > GetScreenHeight()) continue;

            // Lazy load one image per frame
            if (!images[i].loaded && !loadedOneThisFrame) {
                Image img = LoadImage(images[i].path);
                if (img.data != NULL) {
                    ImageResize(&img, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
                    images[i].texture = LoadTextureFromImage(img);
                    UnloadImage(img);
                    images[i].loaded = true;
                }
                loadedOneThisFrame = true;
            }

            if (images[i].loaded) {
                DrawTexture(images[i].texture, x, y, WHITE);
            } else {
                // Placeholder box
                DrawRectangle(x, y, THUMBNAIL_SIZE, THUMBNAIL_SIZE, LIGHTGRAY);
                DrawText("Loading...", x + 10, y + THUMBNAIL_SIZE / 2 - 10, 10, GRAY);
            }
        }

        EndDrawing();
    }

    for (int i = 0; i < imageCount; i++) {
        if (images[i].loaded) UnloadTexture(images[i].texture);
    }

    CloseWindow();
    return 0;
}

