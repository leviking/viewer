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

int LoadFolder(const char* folderPath, ImageEntry* images, int* outCount) {
    DIR* dir = opendir(folderPath);
    if (!dir) return 0;

    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(dir)) && count < MAX_IMAGES) {
        if (!HasImageExtension(entry->d_name)) continue;

        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", folderPath, entry->d_name);
        strncpy(images[count].path, fullPath, sizeof(images[count].path));
        images[count].loaded = false;
        count++;
    }

    closedir(dir);
    *outCount = count;
    return 1;
}

int main(void) {
    InitWindow(1000, 800, "Raylib Lazy Image Viewer");
    SetTargetFPS(60);

    ImageEntry images[MAX_IMAGES];
    int imageCount = 0;
    float scrollY = 0;
    float maxScroll = 0;

    std::string folder = pfd::select_folder("Select a folder of images").result();
    if (folder.empty()) {
        CloseWindow();
        return 0;
    }

    LoadFolder(folder.c_str(), images, &imageCount);

    while (!WindowShouldClose()) {
        // Scroll input
        float wheel = GetMouseWheelMove();
        scrollY += wheel * 30;
        if (scrollY > 0) scrollY = 0;

        int cols = GetScreenWidth() / (THUMBNAIL_SIZE + PADDING);
        int rows = (imageCount + cols - 1) / cols;
        maxScroll = rows * (THUMBNAIL_SIZE + PADDING);
        if (scrollY < GetScreenHeight() - maxScroll - 50)
            scrollY = GetScreenHeight() - maxScroll - 50;

        // Handle "Change Folder" button
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            if (mouse.x >= 10 && mouse.x <= 130 && mouse.y >= 10 && mouse.y <= 40) {
                // Cleanup existing images
                for (int i = 0; i < imageCount; i++) {
                    if (images[i].loaded) {
                        UnloadTexture(images[i].texture);
                        images[i].loaded = false;
                    }
                }
                imageCount = 0;
                scrollY = 0;

                // Reopen dialog
                std::string newFolder = pfd::select_folder("Select a folder of images").result();
                if (!newFolder.empty()) {
                    folder = newFolder;
                    LoadFolder(folder.c_str(), images, &imageCount);
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawRectangle(10, 10, 120, 30, LIGHTGRAY);
        DrawText("Change Folder", 20, 18, 12, DARKGRAY);

        DrawText(TextFormat("Loaded %d / %d", 
            [&]() {
                int n = 0;
                for (int i = 0; i < imageCount; i++) if (images[i].loaded) n++;
                return n;
            }(), imageCount), 150, 18, 16, GRAY);

        bool loadedOneThisFrame = false;

        for (int i = 0; i < imageCount; i++) {
            int x = (i % cols) * (THUMBNAIL_SIZE + PADDING) + PADDING;
            int y = (i / cols) * (THUMBNAIL_SIZE + PADDING) + 50 + scrollY;

            if (y + THUMBNAIL_SIZE < 0 || y > GetScreenHeight()) continue;

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

