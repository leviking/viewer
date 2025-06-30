/**********************************************************************************************
*
*   raylib-viewer - A simple image viewer with canvas layout and PDF export.
*
*   FEATURES:
*       - Thumbnail gallery view for a directory of images.
*       - Full-screen image view with a customizable canvas and margins.
*       - Multi-selection of images in both gallery and full-screen views.
*       - Caching of canvas/margin settings and selected files per directory.
*       - PDF export of selected images with specified canvas layout.
*       - Resizable window with reflowing gallery layout.
*
*   CONTROLS:
*       - Gallery View:
*           - Scroll with mouse wheel.
*           - Left-click a thumbnail to enter full-screen view.
*           - Ctrl/Cmd + Left-click to select/deselect a thumbnail.
*           - "Generate PDF" button appears when images are selected.
*       - Full-Screen View:
*           - Left/Right arrow keys or H/L to navigate between images.
*           - 'S' key to select/deselect the current image.
*           - Escape key or "Back" button to return to the gallery.
*           - UI controls to adjust canvas and margin sizes in inches.
*
*   DEPENDENCIES:
*       - raylib: for graphics and windowing.
*       - raygui: for UI controls.
*       - tinyfiledialogs: for native file dialogs.
*       - pdfgen: for PDF generation.
*
*   LICENSE: This software is licensed under the zlib/libpng license. See LICENSE for details.
*
**********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "tinyfiledialogs.h"
#include "pdfgen.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

//----------------------------------------------------------------------------------
// Defines
//----------------------------------------------------------------------------------
#define DPI 300
#define THUMBNAIL_SIZE 128
#define PADDING 16
#define MAX_IMAGES 4096
#define MAX_PATH_LEN 512
#define MAX_FILENAME_LEN 256
#define MAX_SELECTED_FILES 512

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Represents a single image in the gallery
typedef struct {
    char path[MAX_PATH_LEN];
    Texture2D texture;
    Image fullImage;
    Texture2D fullTexture;
    bool loaded;
    bool fullLoaded;
    bool fullTextureLoaded;
    bool selected;
} ImageEntry;

// Represents the state of the text input boxes for canvas/margin settings
typedef enum {
    TEXTBOX_NONE,
    TEXTBOX_CANVAS_W,
    TEXTBOX_CANVAS_H,
    TEXTBOX_MARGIN_T,
    TEXTBOX_MARGIN_B,
    TEXTBOX_MARGIN_L,
    TEXTBOX_MARGIN_R
} ActiveTextBox;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
bool HasImageExtension(const char *filename);
bool FileExists(const char *path);
void EnsureDirectoryExists(const char *path);
void GetThumbPath(const char *folder, const char *filename, char *outPath);
void SaveSettings(const char* folder, const char* cw, const char* ch, const char* mt, const char* mb, const char* ml, const char* mr, char selectedFiles[MAX_SELECTED_FILES][MAX_FILENAME_LEN], int selectedCount);
void LoadSettings(const char* folder, char* cw, char* ch, char* mt, char* mb, char* ml, char* mr, char selectedFiles[MAX_SELECTED_FILES][MAX_FILENAME_LEN], int* selectedCount);
void PreloadNeighbors(ImageEntry* images, int imageCount, int currentIndex, int* prevIndex, int* nextIndex);
int LoadFolder(const char* folderPath, ImageEntry* images, int* outCount);

//------------------------------------------------------------------------------------
// Program Main Entry Point
//------------------------------------------------------------------------------------
int main(void) {
    // Initialization
    //--------------------------------------------------------------------------------------
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 800, "Raylib Viewer With Canvas");
    SetTargetFPS(60);

    ImageEntry images[MAX_IMAGES];
    int imageCount = 0;
    float scrollY = 0;
    bool inFullView = false;
    int fullViewIndex = -1, prevIndex = -1, nextIndex = -1;

    // Canvas and margin settings (stored as strings for textbox input)
    char bufCanvasW[8] = "8.0", bufCanvasH[8] = "10.0";
    char bufMarginT[8] = "1.0", bufMarginB[8] = "1.0";
    char bufMarginL[8] = "1.0", bufMarginR[8] = "1.0";

    ActiveTextBox activeBox = TEXTBOX_NONE;
    char selectedFiles[MAX_SELECTED_FILES][MAX_FILENAME_LEN];
    int selectedFileCount = 0;

    // Prompt user to select a folder
    const char* folder = tinyfd_selectFolderDialog("Select a folder of images", ".");

    if (!folder || strlen(folder) == 0) {
        CloseWindow();
        return 0;
    }

    // Load settings and image files from the selected folder
    LoadSettings(folder, bufCanvasW, bufCanvasH, bufMarginT, bufMarginB, bufMarginL, bufMarginR, selectedFiles, &selectedFileCount);
    LoadFolder(folder, images, &imageCount);

    // Mark images as selected based on loaded settings
    for (int i = 0; i < imageCount; ++i) {
        const char* filename = GetFileName(images[i].path);
        for (int j = 0; j < selectedFileCount; ++j) {
            if (strcmp(filename, selectedFiles[j]) == 0) {
                images[i].selected = true;
                break;
            }
        }
    }

    // Main game loop
    while (!WindowShouldClose()) {
        // Update
        //----------------------------------------------------------------------------------
        // (No update logic needed here, all handled in drawing section)
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (inFullView && fullViewIndex >= 0 && images[fullViewIndex].fullTextureLoaded) {
            //----------------------------------------------------------------------------------
            // Full-Screen View
            //----------------------------------------------------------------------------------
            ClearBackground(BLACK);

            // Parse canvas and margin values from strings to floats
            float canvasWidthIn = strtof(bufCanvasW, NULL);
            float canvasHeightIn = strtof(bufCanvasH, NULL);
            float marginTopIn = strtof(bufMarginT, NULL);
            float marginBottomIn = strtof(bufMarginB, NULL);
            float marginLeftIn = strtof(bufMarginL, NULL);
            float marginRightIn = strtof(bufMarginR, NULL);

            // Convert inches to pixels based on DPI
            int canvasPxW = canvasWidthIn * DPI;
            int canvasPxH = canvasHeightIn * DPI;
            int marginT = marginTopIn * DPI;
            int marginB = marginBottomIn * DPI;
            int marginL = marginLeftIn * DPI;
            int marginR = marginRightIn * DPI;

            // Scale canvas preview to fit on screen with padding
            float screenPadding = 100.0f;
            float scale = fminf((GetScreenWidth() - screenPadding) / (float)canvasPxW, (GetScreenHeight() - screenPadding) / (float)canvasPxH);
            float dispW = canvasPxW * scale;
            float dispH = canvasPxH * scale;

            // Center the canvas on the screen
            float cx = (GetScreenWidth() - dispW) / 2.0f;
            float cy = (GetScreenHeight() - dispH) / 2.0f;

            DrawRectangle(cx, cy, dispW, dispH, WHITE);
            DrawRectangleLinesEx((Rectangle){cx, cy, dispW, dispH}, 3, GRAY);

            // Calculate scaled margins and drawable area
            float marginL_s = marginL * scale;
            float marginR_s = marginR * scale;
            float marginT_s = marginT * scale;
            float marginB_s = marginB * scale;
            float drawW_s = dispW - marginL_s - marginR_s;
            float drawH_s = dispH - marginT_s - marginB_s;

            // Crop image to fit the drawable area's aspect ratio
            float imgW = images[fullViewIndex].fullImage.width;
            float imgH = images[fullViewIndex].fullImage.height;
            float drawAR = drawW_s / drawH_s;
            float imgAR = imgW / imgH;

            Rectangle src;
            if (imgAR > drawAR) { // Crop width
                float cropW = imgH * drawAR;
                src = (Rectangle){ (imgW - cropW) / 2.0f, 0, cropW, imgH };
            } else { // Crop height
                float cropH = imgW / drawAR;
                src = (Rectangle){ 0, (imgH - cropH) / 2.0f, imgW, cropH };
            }

            Rectangle dst = { cx + marginL_s, cy + marginT_s, drawW_s, drawH_s };
            DrawTexturePro(images[fullViewIndex].fullTexture, src, dst, (Vector2){0, 0}, 0, WHITE);

            // Draw filename at top-center
            const char* filename = GetFileName(images[fullViewIndex].path);
            int textSize = MeasureText(filename, 20);
            DrawText(filename, (GetScreenWidth() - textSize) / 2, 18, 20, WHITE);

            // --- UI Controls ---
            int baseX = 20, baseY = 40, spacing = 40, inputW = 50, inputH = 25;

            // Deactivate textboxes on click outside
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                activeBox = TEXTBOX_NONE;
            }

            // Canvas Size Inputs
            DrawText("Canvas (in)", baseX, baseY, 18, GRAY);
            Rectangle rCW = { (float)baseX, (float)baseY + 20, (float)inputW, (float)inputH };
            if (GuiTextBox(rCW, bufCanvasW, 8, activeBox == TEXTBOX_CANVAS_W)) activeBox = TEXTBOX_CANVAS_W;
            DrawText("W", baseX + inputW + 5, baseY + 25, 16, LIGHTGRAY);
            Rectangle rCH = { (float)baseX + inputW + spacing, (float)baseY + 20, (float)inputW, (float)inputH };
            if (GuiTextBox(rCH, bufCanvasH, 8, activeBox == TEXTBOX_CANVAS_H)) activeBox = TEXTBOX_CANVAS_H;
            DrawText("H", baseX + inputW + spacing + inputW + 5, baseY + 25, 16, LIGHTGRAY);

            // Margin Inputs
            baseY += 60;
            DrawText("Margins (in)", baseX, baseY, 18, GRAY);
            Rectangle rMT = { (float)baseX, (float)baseY + 20, (float)inputW, (float)inputH };
            if (GuiTextBox(rMT, bufMarginT, 8, activeBox == TEXTBOX_MARGIN_T)) activeBox = TEXTBOX_MARGIN_T;
            DrawText("T", baseX + inputW + 5, baseY + 25, 16, LIGHTGRAY);
            Rectangle rMB = { (float)baseX + inputW + spacing, (float)baseY + 20, (float)inputW, (float)inputH };
            if (GuiTextBox(rMB, bufMarginB, 8, activeBox == TEXTBOX_MARGIN_B)) activeBox = TEXTBOX_MARGIN_B;
            DrawText("B", baseX + inputW + spacing + inputW + 5, baseY + 25, 16, LIGHTGRAY);
            Rectangle rML = { (float)baseX, (float)baseY + 20 + inputH + 10, (float)inputW, (float)inputH };
            if (GuiTextBox(rML, bufMarginL, 8, activeBox == TEXTBOX_MARGIN_L)) activeBox = TEXTBOX_MARGIN_L;
            DrawText("L", baseX + inputW + 5, baseY + 25 + inputH + 10, 16, LIGHTGRAY);
            Rectangle rMR = { (float)baseX + inputW + spacing, (float)baseY + 20 + inputH + 10, (float)inputW, (float)inputH };
            if (GuiTextBox(rMR, bufMarginR, 8, activeBox == TEXTBOX_MARGIN_R)) activeBox = TEXTBOX_MARGIN_R;
            DrawText("R", baseX + inputW + spacing + inputW + 5, baseY + 25 + inputH + 10, 16, LIGHTGRAY);

            // "Selected" indicator
            if (images[fullViewIndex].selected) {
                DrawText("Selected", GetScreenWidth() - 120, GetScreenHeight() - 40, 20, BLUE);
            }

            // Toggle selection with 'S' key
            if (IsKeyPressed(KEY_S)) {
                images[fullViewIndex].selected = !images[fullViewIndex].selected;
            }

            // Navigation between images
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_L)) {
                int newIndex = (fullViewIndex + 1) % imageCount;
                UnloadTexture(images[fullViewIndex].fullTexture);
                UnloadImage(images[fullViewIndex].fullImage);
                images[fullViewIndex].fullTextureLoaded = false;
                images[fullViewIndex].fullLoaded = false;
                fullViewIndex = newIndex;
                PreloadNeighbors(images, imageCount, fullViewIndex, &prevIndex, &nextIndex);
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_H)) {
                int newIndex = (fullViewIndex - 1 + imageCount) % imageCount;
                UnloadTexture(images[fullViewIndex].fullTexture);
                UnloadImage(images[fullViewIndex].fullImage);
                images[fullViewIndex].fullTextureLoaded = false;
                images[fullViewIndex].fullLoaded = false;
                fullViewIndex = newIndex;
                PreloadNeighbors(images, imageCount, fullViewIndex, &prevIndex, &nextIndex);
            }

            // Exit full-screen view
            if (IsKeyPressed(KEY_ESCAPE) || GuiButton((Rectangle){ (float)GetScreenWidth() - 120, 40, 100, 30 }, "Back")) {
                // Update selected files list before exiting
                selectedFileCount = 0;
                for (int i = 0; i < imageCount; ++i) {
                    if (images[i].selected) {
                        strncpy(selectedFiles[selectedFileCount], GetFileName(images[i].path), MAX_FILENAME_LEN);
                        selectedFileCount++;
                    }
                }
                SaveSettings(folder, bufCanvasW, bufCanvasH, bufMarginT, bufMarginB, bufMarginL, bufMarginR, selectedFiles, selectedFileCount);
                
                // Unload full-size images
                for (int idx = 0; idx < imageCount; idx++) {
                    if (images[idx].fullTextureLoaded) UnloadTexture(images[idx].fullTexture);
                    if (images[idx].fullLoaded) UnloadImage(images[idx].fullImage);
                    images[idx].fullTextureLoaded = false;
                    images[idx].fullLoaded = false;
                }
                inFullView = false;
            }

        } else {
            //----------------------------------------------------------------------------------
            // Thumbnail Gallery View
            //----------------------------------------------------------------------------------
            
            // --- Title Bar ---
            Rectangle titleBar = { 0, 0, (float)GetScreenWidth(), 50 };
            DrawRectangleRec(titleBar, RAYWHITE);
            DrawLine(0, (int)titleBar.height, GetScreenWidth(), (int)titleBar.height, LIGHTGRAY);

            // Draw folder name on title bar
            const char* dirName = GetFileName(folder);
            int dirNameWidth = MeasureText(dirName, 20);
            DrawText(dirName, (GetScreenWidth() - dirNameWidth) / 2, (int)(titleBar.height - 20) / 2, 20, BLACK);

            // Draw folder name on title bar
            const char* dirName = GetFileName(folder);
            int dirNameWidth = MeasureText(dirName, 20);
            DrawText(dirName, (GetScreenWidth() - dirNameWidth) / 2, (int)(titleBar.height - 20) / 2, 20, BLACK);

            // Draw "Generate PDF" button if images are selected
            int selectedCount = 0;
            for (int i = 0; i < imageCount; i++) {
                if (images[i].selected) selectedCount++;
            }
            if (selectedCount > 0) {
                if (GuiButton((Rectangle){ (float)GetScreenWidth() - 150, (titleBar.height - 30) / 2, 120, 30 }, "Generate PDF")) {
                    const char* filterPatterns[] = { "*.pdf" };
                    const char* pdfPath = tinyfd_saveFileDialog("Save PDF", "output.pdf", 1, filterPatterns, "PDF Files");

                    if (pdfPath && strlen(pdfPath) > 0) {
                        // PDF generation logic...
                        float cw = strtof(bufCanvasW, NULL);
                        float ch = strtof(bufCanvasH, NULL);
                        float mt = strtof(bufMarginT, NULL);
                        float mb = strtof(bufMarginB, NULL);
                        float ml = strtof(bufMarginL, NULL);
                        float mr = strtof(bufMarginR, NULL);

                        float pageW = cw * 72.0f;
                        float pageH = ch * 72.0f;

                        struct pdf_info info = {
                            .creator = "Raylib Viewer",
                            .producer = "PDFGen",
                            .title = "Image Compilation",
                            .author = "User",
                            .subject = "Selected Images",
                            .date = "Today"
                        };
                        struct pdf_doc *pdf = pdf_create(pageW, pageH, &info);
                        pdf_set_font(pdf, "Helvetica");

                        float drawX = ml * 72.0f;
                        float drawY = mb * 72.0f;
                        float drawW = (cw - ml - mr) * 72.0f;
                        float drawH = (ch - mt - mb) * 72.0f;

                        for (int i = 0; i < imageCount; i++) {
                            if (images[i].selected) {
                                Image img = LoadImage(images[i].path);
                                if (img.data) {
                                    pdf_append_page(pdf);
                                    float imgW = img.width;
                                    float imgH = img.height;
                                    float scale = fminf(drawW / imgW, drawH / imgH);
                                    float finalW = imgW * scale;
                                    float finalH = imgH * scale;
                                    float finalX = drawX + (drawW - finalW) / 2.0f;
                                    float finalY = drawY + (drawH - finalH) / 2.0f;
                                    pdf_add_image_file(pdf, NULL, finalX, finalY, finalW, finalH, images[i].path);
                                    UnloadImage(img);
                                }
                            }
                        }
                        pdf_save(pdf, pdfPath);
                        pdf_destroy(pdf);
                    }
                }
            }

            // --- Scrolling Thumbnail Area ---
            BeginScissorMode(0, (int)titleBar.height + 1, GetScreenWidth(), GetScreenHeight() - (int)titleBar.height - 1);
            
            // Calculate centered gallery layout
            int cols = (GetScreenWidth() / (THUMBNAIL_SIZE + PADDING));
            if (cols == 0) cols = 1;
            if (cols > imageCount) cols = imageCount;
            int content_width = cols * THUMBNAIL_SIZE + (cols - 1) * PADDING;
            int startX = (GetScreenWidth() - content_width) / 2;
            int rows = (imageCount + cols - 1) / cols;

            // Handle mouse wheel scrolling
            float wheel = GetMouseWheelMove();
            scrollY += wheel * 30;
            if (scrollY > 0) scrollY = 0;
            
            float scrollAreaHeight = GetScreenHeight() - titleBar.height;
            float totalContentHeight = rows * (THUMBNAIL_SIZE + PADDING) + PADDING;
            float maxScroll = 0;
            if (totalContentHeight > scrollAreaHeight) {
                maxScroll = totalContentHeight - scrollAreaHeight;
            }
            if (scrollY < -maxScroll) scrollY = -maxScroll;

            bool loadedOne = false; // Flag to load one thumbnail per frame

            for (int i = 0; i < imageCount; i++) {
                int x = startX + (i % cols) * (THUMBNAIL_SIZE + PADDING);
                int y = (i / cols) * (THUMBNAIL_SIZE + PADDING) + (int)titleBar.height + PADDING + (int)scrollY;
                
                // Culling: Don't process thumbnails that are off-screen
                if (y + THUMBNAIL_SIZE < titleBar.height || y > GetScreenHeight()) continue;

                // Lazy-load thumbnails
                if (!images[i].loaded && !loadedOne) {
                    char thumbPath[MAX_PATH_LEN];
                    GetThumbPath(folder, GetFileName(images[i].path), thumbPath);
                    
                    if (FileExists(thumbPath)) {
                        Image thumb = LoadImage(thumbPath);
                        images[i].texture = LoadTextureFromImage(thumb);
                        UnloadImage(thumb);
                    } else {
                        Image full = LoadImage(images[i].path);
                        if (full.data) {
                            float aspect = (float)THUMBNAIL_SIZE / fmaxf(full.width, full.height);
                            ImageResize(&full, full.width * aspect, full.height * aspect);
                            ExportImage(full, thumbPath);
                            images[i].texture = LoadTextureFromImage(full);
                            UnloadImage(full);
                        }
                    }
                    images[i].loaded = true;
                    loadedOne = true;
                }

                if (images[i].loaded) {
                    // Draw thumbnail with border
                    DrawRectangle(x, y, THUMBNAIL_SIZE, THUMBNAIL_SIZE, LIGHTGRAY);
                    float scale = fminf((float)THUMBNAIL_SIZE / images[i].texture.width, (float)THUMBNAIL_SIZE / images[i].texture.height);
                    float w = images[i].texture.width * scale;
                    float h = images[i].texture.height * scale;
                    float tx = x + (THUMBNAIL_SIZE - w) / 2;
                    float ty = y + (THUMBNAIL_SIZE - h) / 2;
                    DrawTexturePro(images[i].texture, (Rectangle){0,0, (float)images[i].texture.width, (float)images[i].texture.height}, (Rectangle){tx, ty, w, h}, (Vector2){0,0}, 0, WHITE);
                    DrawRectangleLinesEx((Rectangle){(float)x, (float)y, THUMBNAIL_SIZE, THUMBNAIL_SIZE}, 3, images[i].selected ? BLUE : GRAY);

                    // Handle mouse clicks for selection and full-screen view
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        Vector2 mouse = GetMousePosition();
                        Rectangle r = {(float)x, (float)y, (float)THUMBNAIL_SIZE, (float)THUMBNAIL_SIZE};
                        if (CheckCollisionPointRec(mouse, r)) {
                            if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) {
                                images[i].selected = !images[i].selected;
                            } else {
                                Image img = LoadImage(images[i].path);
                                if (img.data != NULL) {
                                    images[i].fullImage = img;
                                    images[i].fullLoaded = true;
                                    images[i].fullTexture = LoadTextureFromImage(img);
                                    images[i].fullTextureLoaded = true;
                                    inFullView = true;
                                    fullViewIndex = i;
                                    PreloadNeighbors(images, imageCount, i, &prevIndex, &nextIndex);
                                }
                            }
                        }
                    }
                }
            }
            EndScissorMode();
        }

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Save settings on exit
    selectedFileCount = 0;
    for (int i = 0; i < imageCount; ++i) {
        if (images[i].selected) {
            strncpy(selectedFiles[selectedFileCount], GetFileName(images[i].path), MAX_FILENAME_LEN -1);
            selectedFiles[selectedFileCount][MAX_FILENAME_LEN - 1] = '\0'; // Ensure null termination
            selectedFileCount++;
        }
    }
    SaveSettings(folder, bufCanvasW, bufCanvasH, bufMarginT, bufMarginB, bufMarginL, bufMarginR, selectedFiles, selectedFileCount);

    // Unload all textures and images
    for (int i = 0; i < imageCount; i++) {
        if (images[i].loaded) UnloadTexture(images[i].texture);
        if (images[i].fullTextureLoaded) UnloadTexture(images[i].fullTexture);
        if (images[i].fullLoaded) UnloadImage(images[i].fullImage);
    }

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Check if a filename has a common image extension
bool HasImageExtension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".png") == 0 ||
            strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0);
}

// Check if a file exists on the filesystem
bool FileExists(const char *path) {
    return access(path, F_OK) != -1;
}

// Create a directory if it doesn't already exist
void EnsureDirectoryExists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

// Construct the path for a thumbnail image
void GetThumbPath(const char *folder, const char *filename, char *outPath) {
    char thumbDir[MAX_PATH_LEN];
    snprintf(thumbDir, MAX_PATH_LEN, "%s/.rayview_thumbs", folder);
    EnsureDirectoryExists(thumbDir);
    snprintf(outPath, MAX_PATH_LEN, "%s/%s__thumb.png", thumbDir, filename);
}

// Save canvas/margin settings and selected filenames to a .rayview_settings file
void SaveSettings(const char* folder, const char* cw, const char* ch, const char* mt, const char* mb, const char* ml, const char* mr, char selectedFiles[MAX_SELECTED_FILES][MAX_FILENAME_LEN], int selectedCount) {
    char settingsPath[MAX_PATH_LEN];
    snprintf(settingsPath, MAX_PATH_LEN, "%s/.rayview_settings", folder);
    FILE* f = fopen(settingsPath, "w");
    if (f) {
        fprintf(f, "%s\n%s\n%s\n%s\n%s\n%s\n", cw, ch, mt, mb, ml, mr);
        for (int i = 0; i < selectedCount; ++i) {
            fprintf(f, "%s\n", selectedFiles[i]);
        }
        fclose(f);
    }
}

// Load settings from the .rayview_settings file
void LoadSettings(const char* folder, char* cw, char* ch, char* mt, char* mb, char* ml, char* mr, char selectedFiles[MAX_SELECTED_FILES][MAX_FILENAME_LEN], int* selectedCount) {
    char settingsPath[MAX_PATH_LEN];
    snprintf(settingsPath, MAX_PATH_LEN, "%s/.rayview_settings", folder);
    FILE* f = fopen(settingsPath, "r");
    if (f) {
        // Use dummy reads to avoid warnings, but check return value
        if (fscanf(f, "%s\n%s\n%s\n%s\n%s\n%s\n", cw, ch, mt, mb, ml, mr) != 6) {
            // Handle error or incomplete file
        }
        *selectedCount = 0;
        while (*selectedCount < MAX_SELECTED_FILES && fscanf(f, "%s\n", selectedFiles[*selectedCount]) != EOF) {
            (*selectedCount)++;
        }
        fclose(f);
    }
}

// Preload the full-size images for the next and previous images in the list
void PreloadNeighbors(ImageEntry* images, int imageCount, int currentIndex, int* prevIndex, int* nextIndex) {
    *prevIndex = (currentIndex - 1 + imageCount) % imageCount;
    *nextIndex = (currentIndex + 1) % imageCount;

    int indices[] = {*prevIndex, *nextIndex};
    for (int i = 0; i < 2; ++i) {
        int idx = indices[i];
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

// Load all image files from a directory into the ImageEntry array
int LoadFolder(const char* folderPath, ImageEntry* images, int* outCount) {
    DIR* dir = opendir(folderPath);
    if (!dir) return 0;

    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL && count < MAX_IMAGES) {
        if (!HasImageExtension(entry->d_name)) continue;

        snprintf(images[count].path, MAX_PATH_LEN, "%s/%s", folderPath, entry->d_name);
        images[count].loaded = false;
        images[count].fullLoaded = false;
        images[count].fullTextureLoaded = false;
        images[count].selected = false;
        count++;
    }

    closedir(dir);
    *outCount = count;
    return 1;
}
