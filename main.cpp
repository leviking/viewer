#include "raylib.h"
#define PFD_IMPLEMENTATION
#include "portable-file-dialogs.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>   // mkdir
#include <unistd.h>     // access
#include <string>

#define THUMBNAIL_SIZE 128
#define PADDING 16
#define MAX_IMAGES 4096

typedef struct {
    char path[512];
    Texture2D texture;
    Image fullImage;
    Texture2D fullTexture;
    bool loaded;
    bool fullLoaded;
    bool fullTextureLoaded;
} ImageEntry;

bool HasImageExtension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".png") == 0 ||
            strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0);
}

bool FileExistsSys(const char *path) {
    return access(path, F_OK) != -1;
}

void EnsureDirectoryExists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

std::string GetThumbPath(const std::string &folder, const std::string &filename) {
    std::string thumbDir = folder + "/.rayview_thumbs";
    EnsureDirectoryExists(thumbDir.c_str());
    return thumbDir + "/" + filename + "__thumb.png";
}

void PreloadNeighbors(ImageEntry* images, int imageCount, int currentIndex, int& prevIndex, int& nextIndex) {
    prevIndex = (currentIndex - 1 + imageCount) % imageCount;
    nextIndex = (currentIndex + 1) % imageCount;

    if (!images[prevIndex].fullLoaded) {
        Image img = LoadImage(images[prevIndex].path);
        if (img.data != NULL) {
            images[prevIndex].fullImage = img;
            images[prevIndex].fullLoaded = true;
            images[prevIndex].fullTexture = LoadTextureFromImage(img);
            images[prevIndex].fullTextureLoaded = true;
        }
    }

    if (!images[nextIndex].fullLoaded) {
        Image img = LoadImage(images[nextIndex].path);
        if (img.data != NULL) {
            images[nextIndex].fullImage = img;
            images[nextIndex].fullLoaded = true;
            images[nextIndex].fullTexture = LoadTextureFromImage(img);
            images[nextIndex].fullTextureLoaded = true;
        }
    }
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
        images[count].fullLoaded = false;
        images[count].fullTextureLoaded = false;
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

    bool inFullView = false;
    int fullViewIndex = -1;
    int prevIndex = -1;
    int nextIndex = -1;

    std::string folder = pfd::select_folder("Select a folder of images").result();
    if (folder.empty()) {
        CloseWindow();
        return 0;
    }

    LoadFolder(folder.c_str(), images, &imageCount);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (inFullView && fullViewIndex >= 0 && fullViewIndex < imageCount &&
            images[fullViewIndex].fullTextureLoaded) {

            ClearBackground(BLACK);

            float scale = fmin(
                (float)GetScreenWidth() / images[fullViewIndex].fullImage.width,
                (float)GetScreenHeight() / images[fullViewIndex].fullImage.height
            );
            float w = images[fullViewIndex].fullImage.width * scale;
            float h = images[fullViewIndex].fullImage.height * scale;
            float x = (GetScreenWidth() - w) / 2;
            float y = (GetScreenHeight() - h) / 2;

            DrawTextureEx(images[fullViewIndex].fullTexture, (Vector2){x, y}, 0, scale, WHITE);
            DrawText("←/→ h/l to navigate, click or ESC to close", 20, GetScreenHeight() - 30, 16, LIGHTGRAY);

            const char* filename = GetFileName(images[fullViewIndex].path);
            int textSize = MeasureText(filename, 20);
            DrawRectangle(10, 10, textSize + 20, 34, Fade(BLACK, 0.5f)); // semi-opaque background
            DrawText(filename, 21, 19, 20, BLACK);
            DrawText(filename, 20, 18, 20, WHITE);


            // Handle navigation
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_L)) {
                int newIndex = nextIndex;

                if (images[fullViewIndex].fullTextureLoaded)
                    UnloadTexture(images[fullViewIndex].fullTexture);
                if (images[fullViewIndex].fullLoaded)
                    UnloadImage(images[fullViewIndex].fullImage);

                images[fullViewIndex].fullTextureLoaded = false;
                images[fullViewIndex].fullLoaded = false;

                fullViewIndex = newIndex;
                PreloadNeighbors(images, imageCount, fullViewIndex, prevIndex, nextIndex);
            }

            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_H)) {
                int newIndex = prevIndex;

                if (images[fullViewIndex].fullTextureLoaded)
                    UnloadTexture(images[fullViewIndex].fullTexture);
                if (images[fullViewIndex].fullLoaded)
                    UnloadImage(images[fullViewIndex].fullImage);

                images[fullViewIndex].fullTextureLoaded = false;
                images[fullViewIndex].fullLoaded = false;

                fullViewIndex = newIndex;
                PreloadNeighbors(images, imageCount, fullViewIndex, prevIndex, nextIndex);
            }

            if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                inFullView = false;

                for (int idx : {fullViewIndex, nextIndex, prevIndex}) {
                    if (images[idx].fullTextureLoaded) {
                        UnloadTexture(images[idx].fullTexture);
                        images[idx].fullTextureLoaded = false;
                    }
                    if (images[idx].fullLoaded) {
                        UnloadImage(images[idx].fullImage);
                        images[idx].fullLoaded = false;
                    }
                }

                fullViewIndex = prevIndex = nextIndex = -1;
            }

            EndDrawing();
            continue;
        }

        // Thumbnail mode
        float wheel = GetMouseWheelMove();
        scrollY += wheel * 30;
        if (scrollY > 0) scrollY = 0;

        int cols = GetScreenWidth() / (THUMBNAIL_SIZE + PADDING);
        int rows = (imageCount + cols - 1) / cols;
        maxScroll = rows * (THUMBNAIL_SIZE + PADDING);
        if (scrollY < GetScreenHeight() - maxScroll - 50)
            scrollY = GetScreenHeight() - maxScroll - 50;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            if (mouse.x >= 10 && mouse.x <= 130 && mouse.y >= 10 && mouse.y <= 40) {
                for (int i = 0; i < imageCount; i++) {
                    if (images[i].loaded) UnloadTexture(images[i].texture);
                    if (images[i].fullTextureLoaded) UnloadTexture(images[i].fullTexture);
                    if (images[i].fullLoaded) UnloadImage(images[i].fullImage);
                }
                imageCount = 0;
                scrollY = 0;

                std::string newFolder = pfd::select_folder("Select a folder of images").result();
                if (!newFolder.empty()) {
                    folder = newFolder;
                    LoadFolder(folder.c_str(), images, &imageCount);
                }
            }
        }

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
                std::string baseName = GetFileName(images[i].path);
                std::string thumbPath = GetThumbPath(folder, baseName);

                if (FileExistsSys(thumbPath.c_str())) {
                    Image thumb = LoadImage(thumbPath.c_str());
                    images[i].texture = LoadTextureFromImage(thumb);
                    UnloadImage(thumb);
                } else {
                    Image full = LoadImage(images[i].path);
                    if (full.data != NULL) {
                        float scale = fminf(
                            (float)THUMBNAIL_SIZE / full.width,
                            (float)THUMBNAIL_SIZE / full.height
                        );
                        int w = full.width * scale;
                        int h = full.height * scale;
                        ImageResize(&full, w, h);
                        ExportImage(full, thumbPath.c_str());
                        images[i].texture = LoadTextureFromImage(full);
                        UnloadImage(full);
                    }
                }
                images[i].loaded = true;
                loadedOneThisFrame = true;
            }

            if (images[i].loaded) {
                DrawRectangle(x, y, THUMBNAIL_SIZE, THUMBNAIL_SIZE, LIGHTGRAY);

                float scale = fminf(
                    (float)THUMBNAIL_SIZE / images[i].texture.width,
                    (float)THUMBNAIL_SIZE / images[i].texture.height
                );
                float w = images[i].texture.width * scale;
                float h = images[i].texture.height * scale;
                float tx = x + (THUMBNAIL_SIZE - w) / 2;
                float ty = y + (THUMBNAIL_SIZE - h) / 2;

                Rectangle src = { 0, 0, (float)images[i].texture.width, (float)images[i].texture.height };
                Rectangle dst = { tx, ty, w, h };

                DrawTexturePro(images[i].texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
                DrawRectangleLines(x, y, THUMBNAIL_SIZE, THUMBNAIL_SIZE, GRAY);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mouse = GetMousePosition();
                    Rectangle thumbRect = {(float)x, (float)y, (float)THUMBNAIL_SIZE, (float)THUMBNAIL_SIZE};
                    if (CheckCollisionPointRec(mouse, thumbRect)) {
                        Image img = LoadImage(images[i].path);
                        if (img.data != NULL) {
                            images[i].fullImage = img;
                            images[i].fullLoaded = true;
                            images[i].fullTexture = LoadTextureFromImage(img);
                            images[i].fullTextureLoaded = true;
                            inFullView = true;
                            fullViewIndex = i;
                            PreloadNeighbors(images, imageCount, i, prevIndex, nextIndex);
                        }
                    }
                }
            } else {
                DrawRectangle(x, y, THUMBNAIL_SIZE, THUMBNAIL_SIZE, LIGHTGRAY);
                DrawText("Loading...", x + 10, y + THUMBNAIL_SIZE / 2 - 10, 10, GRAY);
            }
        }

        EndDrawing();
    }

    for (int i = 0; i < imageCount; i++) {
        if (images[i].loaded) UnloadTexture(images[i].texture);
        if (images[i].fullTextureLoaded) UnloadTexture(images[i].fullTexture);
        if (images[i].fullLoaded) UnloadImage(images[i].fullImage);
    }

    CloseWindow();
    return 0;
}

