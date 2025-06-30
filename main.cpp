#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#define PFD_IMPLEMENTATION
#include "portable-file-dialogs.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

#define DPI 300
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

    for (int idx : {prevIndex, nextIndex}) {
        if (!images[idx].fullLoaded) {
            Image img = LoadImage(images[idx].path);
            if (img.data != NULL) {
                images[idx].fullImage = img;
                images[idx].fullLoaded = true;
                images[idx].fullTexture = LoadTextureFromImage(img);
                images[idx].fullTextureLoaded = true;
            }
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
    InitWindow(1000, 800, "Raylib Viewer With Canvas");
    SetTargetFPS(60);

    ImageEntry images[MAX_IMAGES];
    int imageCount = 0;
    float scrollY = 0;
    bool inFullView = false;
    int fullViewIndex = -1, prevIndex = -1, nextIndex = -1;

    // Canvas and margin inputs (in inches)
    float canvasWidthIn = 8.0f, canvasHeightIn = 10.0f;
    float marginTopIn = 1.0f, marginBottomIn = 1.0f;
    float marginLeftIn = 1.0f, marginRightIn = 1.0f;

    char bufCanvasW[8] = "8.0", bufCanvasH[8] = "10.0";
    char bufMarginT[8] = "1.0", bufMarginB[8] = "1.0";
    char bufMarginL[8] = "1.0", bufMarginR[8] = "1.0";

    enum ActiveTextBox { NONE, CW, CH, MT, MB, ML, MR };
    static ActiveTextBox activeBox = NONE;

    std::string folder = pfd::select_folder("Select a folder of images").result();
    if (folder.empty()) { CloseWindow(); return 0; }

    LoadFolder(folder.c_str(), images, &imageCount);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (inFullView && fullViewIndex >= 0 && images[fullViewIndex].fullTextureLoaded) {
            ClearBackground(BLACK);

            // Parse inches to floats
            canvasWidthIn = strtof(bufCanvasW, NULL);
            canvasHeightIn = strtof(bufCanvasH, NULL);
            marginTopIn = strtof(bufMarginT, NULL);
            marginBottomIn = strtof(bufMarginB, NULL);
            marginLeftIn = strtof(bufMarginL, NULL);
            marginRightIn = strtof(bufMarginR, NULL);

            int canvasPxW = canvasWidthIn * DPI;
            int canvasPxH = canvasHeightIn * DPI;
            int marginT = marginTopIn * DPI;
            int marginB = marginBottomIn * DPI;
            int marginL = marginLeftIn * DPI;
            int marginR = marginRightIn * DPI;

            // Calculate scale to fit canvas onto screen, with padding
            float screenPadding = 100.0f;
            float scale = fminf((GetScreenWidth() - screenPadding) / (float)canvasPxW, (GetScreenHeight() - screenPadding) / (float)canvasPxH);

            float dispW = canvasPxW * scale;
            float dispH = canvasPxH * scale;

            // Center canvas
            float cx = (GetScreenWidth() - dispW) / 2.0f;
            float cy = (GetScreenHeight() - dispH) / 2.0f;

            DrawRectangle(cx, cy, dispW, dispH, WHITE);
            DrawRectangleLines(cx, cy, dispW, dispH, GRAY);

            float marginL_s = marginL * scale;
            float marginR_s = marginR * scale;
            float marginT_s = marginT * scale;
            float marginB_s = marginB * scale;

            float drawW_s = dispW - marginL_s - marginR_s;
            float drawH_s = dispH - marginT_s - marginB_s;

            // Crop to fit image aspect into drawable area
            float imgW = images[fullViewIndex].fullImage.width;
            float imgH = images[fullViewIndex].fullImage.height;
            float drawAR = drawW_s / drawH_s;
            float imgAR = imgW / imgH;

            Rectangle src;
            if (imgAR > drawAR) {
                // Crop width
                float cropW = imgH * drawAR;
                float offsetX = (imgW - cropW) / 2.0f;
                src = { offsetX, 0, cropW, imgH };
            } else {
                // Crop height
                float cropH = imgW / drawAR;
                float offsetY = (imgH - cropH) / 2.0f;
                src = { 0, offsetY, imgW, cropH };
            }

            Rectangle dst = {
                cx + marginL_s,
                cy + marginT_s,
                drawW_s,
                drawH_s
            };

            DrawTexturePro(images[fullViewIndex].fullTexture, src, dst, {0, 0}, 0, WHITE);

            // Draw filename
            const char* filename = GetFileName(images[fullViewIndex].path);
            int textSize = MeasureText(filename, 20);
            int textX = (GetScreenWidth() - textSize) / 2;
            DrawText(filename, textX + 1, 19, 20, BLACK);
            DrawText(filename, textX, 18, 20, WHITE);

            // --- UI Controls ---
            int baseX = 20, baseY = 40, spacing = 40, inputW = 50, inputH = 25, labelW = 20;

            // Deactivate all boxes on click outside
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                activeBox = NONE;
            }

            // Canvas Size
            DrawText("Canvas (in)", baseX, baseY, 18, GRAY);
            Rectangle rCW = { (float)baseX, (float)baseY + 20, (float)inputW, (float)inputH };
            Rectangle rCH = { (float)baseX + inputW + spacing, (float)baseY + 20, (float)inputW, (float)inputH };
            DrawText("W", baseX + inputW + 5, baseY + 25, 16, LIGHTGRAY);
            if (GuiTextBox(rCW, bufCanvasW, 8, activeBox == CW)) activeBox = CW;
            DrawText("H", baseX + inputW + spacing + inputW + 5, baseY + 25, 16, LIGHTGRAY);
            if (GuiTextBox(rCH, bufCanvasH, 8, activeBox == CH)) activeBox = CH;

            // Margins
            baseY += 60;
            DrawText("Margins (in)", baseX, baseY, 18, GRAY);
            Rectangle rMT = { (float)baseX, (float)baseY + 20, (float)inputW, (float)inputH };
            Rectangle rMB = { (float)baseX + inputW + spacing, (float)baseY + 20, (float)inputW, (float)inputH };
            Rectangle rML = { (float)baseX, (float)baseY + 20 + inputH + 10, (float)inputW, (float)inputH };
            Rectangle rMR = { (float)baseX + inputW + spacing, (float)baseY + 20 + inputH + 10, (float)inputW, (float)inputH };

            DrawText("T", baseX + inputW + 5, baseY + 25, 16, LIGHTGRAY);
            if (GuiTextBox(rMT, bufMarginT, 8, activeBox == MT)) activeBox = MT;
            DrawText("B", baseX + inputW + spacing + inputW + 5, baseY + 25, 16, LIGHTGRAY);
            if (GuiTextBox(rMB, bufMarginB, 8, activeBox == MB)) activeBox = MB;
            DrawText("L", baseX + inputW + 5, baseY + 25 + inputH + 10, 16, LIGHTGRAY);
            if (GuiTextBox(rML, bufMarginL, 8, activeBox == ML)) activeBox = ML;
            DrawText("R", baseX + inputW + spacing + inputW + 5, baseY + 25 + inputH + 10, 16, LIGHTGRAY);
            if (GuiTextBox(rMR, bufMarginR, 8, activeBox == MR)) activeBox = MR;

            // Navigation
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_L)) {
                int newIndex = (fullViewIndex + 1) % imageCount;
                UnloadTexture(images[fullViewIndex].fullTexture);
                UnloadImage(images[fullViewIndex].fullImage);
                images[fullViewIndex].fullTextureLoaded = false;
                images[fullViewIndex].fullLoaded = false;
                fullViewIndex = newIndex;
                PreloadNeighbors(images, imageCount, fullViewIndex, prevIndex, nextIndex);
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_H)) {
                int newIndex = (fullViewIndex - 1 + imageCount) % imageCount;
                UnloadTexture(images[fullViewIndex].fullTexture);
                UnloadImage(images[fullViewIndex].fullImage);
                images[fullViewIndex].fullTextureLoaded = false;
                images[fullViewIndex].fullLoaded = false;
                fullViewIndex = newIndex;
                PreloadNeighbors(images, imageCount, fullViewIndex, prevIndex, nextIndex);
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                for (int idx : {fullViewIndex, prevIndex, nextIndex}) {
                    if (images[idx].fullTextureLoaded) UnloadTexture(images[idx].fullTexture);
                    if (images[idx].fullLoaded) UnloadImage(images[idx].fullImage);
                    images[idx].fullTextureLoaded = false;
                    images[idx].fullLoaded = false;
                }
                inFullView = false;
            }

        } else {
            // Thumbnail layout
            int cols = GetScreenWidth() / (THUMBNAIL_SIZE + PADDING);
            int rows = (imageCount + cols - 1) / cols;
            float wheel = GetMouseWheelMove();
            scrollY += wheel * 30;
            if (scrollY > 0) scrollY = 0;
            float maxScroll = rows * (THUMBNAIL_SIZE + PADDING);
            if (scrollY < GetScreenHeight() - maxScroll - 50)
                scrollY = GetScreenHeight() - maxScroll - 50;

            bool loadedOne = false;

            for (int i = 0; i < imageCount; i++) {
                int x = (i % cols) * (THUMBNAIL_SIZE + PADDING) + PADDING;
                int y = (i / cols) * (THUMBNAIL_SIZE + PADDING) + 50 + scrollY;
                if (y + THUMBNAIL_SIZE < 0 || y > GetScreenHeight()) continue;

                if (!images[i].loaded && !loadedOne) {
                    std::string base = GetFileName(images[i].path);
                    std::string thumbPath = GetThumbPath(folder, base);
                    if (FileExistsSys(thumbPath.c_str())) {
                        Image thumb = LoadImage(thumbPath.c_str());
                        images[i].texture = LoadTextureFromImage(thumb);
                        UnloadImage(thumb);
                    } else {
                        Image full = LoadImage(images[i].path);
                        if (full.data) {
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
                    loadedOne = true;
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
                    DrawTexturePro(images[i].texture, src, dst, {0,0}, 0, WHITE);
                    DrawRectangleLines(x, y, THUMBNAIL_SIZE, THUMBNAIL_SIZE, GRAY);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        Vector2 mouse = GetMousePosition();
                        Rectangle r = {(float)x, (float)y, (float)THUMBNAIL_SIZE, (float)THUMBNAIL_SIZE};
                        if (CheckCollisionPointRec(mouse, r)) {
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
                }
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

