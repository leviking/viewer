#include "ui.h"
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "tinyfiledialogs.h"
#include "pdfgen.h"
#include "state.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

extern bool FileExists(const char *path);
extern void GetThumbPath(const char *folder, const char *filename, char *outPath);

void DrawGalleryView(State* state) {
    Rectangle titleBar = { 0, 0, (float)GetScreenWidth(), 50 };
    DrawRectangleRec(titleBar, RAYWHITE);
    DrawLine(0, (int)titleBar.height, GetScreenWidth(), (int)titleBar.height, LIGHTGRAY);

    if (GuiButton((Rectangle){ 10, (titleBar.height - 30) / 2, 120, 30 }, "Change Folder")) {
        const char* newFolder = tinyfd_selectFolderDialog("Select a new folder", state->folder);
        if (newFolder && strlen(newFolder) > 0 && strcmp(newFolder, state->folder) != 0) {
            for (int i = 0; i < state->imageCount; i++) {
                if (state->images[i].loaded) UnloadTexture(state->images[i].texture);
                if (state->images[i].fullTextureLoaded) UnloadTexture(state->images[i].fullTexture);
                if (state->images[i].fullLoaded) UnloadImage(state->images[i].fullImage);
            }
            strncpy(state->folder, newFolder, MAX_PATH_LEN - 1);
            state->folder[MAX_PATH_LEN - 1] = '\0';
            LoadSettings(state);
            LoadFolder(state, state->folder);
            for (int i = 0; i < state->imageCount; ++i) {
                state->images[i].selectionOrder = -1;
                const char* filename = GetFileName(state->images[i].path);
                for (int j = 0; j < state->selectedFileCount; ++j) {
                    if (strcmp(filename, state->selectedFiles[j]) == 0) {
                        state->images[i].selected = true;
                        state->images[i].selectionOrder = j + 1;
                        break;
                    }
                }
            }
            state->scrollY = 0;
        }
    }

    char cleanPath[MAX_PATH_LEN];
    strncpy(cleanPath, state->folder, MAX_PATH_LEN - 1);
    cleanPath[MAX_PATH_LEN - 1] = '\0';
    int len = strlen(cleanPath);
    if (len > 0 && (cleanPath[len - 1] == '/' || cleanPath[len - 1] == '\\')) {
        cleanPath[len - 1] = '\0';
    }
    const char* dirName = GetFileName(cleanPath);
    int dirNameWidth = MeasureTextEx(state->font, dirName, 20, 1).x;
    DrawTextEx(state->font, dirName, (Vector2){(GetScreenWidth() - dirNameWidth) / 2, (int)(titleBar.height - 20) / 2}, 20, 1, BLACK);

    int selectedCount = 0;
    for (int i = 0; i < state->imageCount; i++) {
        if (state->images[i].selected) selectedCount++;
    }
    if (selectedCount > 0) {
        if (GuiButton((Rectangle){ (float)GetScreenWidth() - 180, (titleBar.height - 30) / 2, 150, 30 }, "Reorder/Export")) {
            state->currentState = STATE_REORDER;
        }
    }

    BeginScissorMode(0, (int)titleBar.height + 1, GetScreenWidth(), GetScreenHeight() - (int)titleBar.height - 1);
    
    int cols = (GetScreenWidth() / (128 + 16));
    if (cols == 0) cols = 1;
    if (cols > state->imageCount) cols = state->imageCount;
    int content_width = cols * 128 + (cols - 1) * 16;
    int startX = (GetScreenWidth() - content_width) / 2;
    int rows = (state->imageCount + cols - 1) / cols;

    float wheel = GetMouseWheelMove();
    state->scrollY += wheel * 30;
    if (state->scrollY > 0) state->scrollY = 0;
    
    float scrollAreaHeight = GetScreenHeight() - titleBar.height;
    float totalContentHeight = rows * (128 + 16) + 16;
    float maxScroll = 0;
    if (totalContentHeight > scrollAreaHeight) {
        maxScroll = totalContentHeight - scrollAreaHeight;
    }
    if (state->scrollY < -maxScroll) state->scrollY = -maxScroll;

    bool loadedOne = false;

    for (int i = 0; i < state->imageCount; i++) {
        int x = startX + (i % cols) * (128 + 16);
        int y = (i / cols) * (128 + 16) + (int)titleBar.height + 16 + (int)state->scrollY;
        
        if (y + 128 < titleBar.height || y > GetScreenHeight()) continue;

        if (!state->images[i].loaded && !loadedOne) {
            char thumbPath[MAX_PATH_LEN];
            GetThumbPath(state->folder, GetFileName(state->images[i].path), thumbPath);
            
            if (FileExists(thumbPath)) {
                Image thumb = LoadImage(thumbPath);
                state->images[i].texture = LoadTextureFromImage(thumb);
                UnloadImage(thumb);
            } else {
                Image full = LoadImage(state->images[i].path);
                if (full.data) {
                    float aspect = (float)128 / fmaxf(full.width, full.height);
                    ImageResize(&full, full.width * aspect, full.height * aspect);
                    ExportImage(full, thumbPath);
                    state->images[i].texture = LoadTextureFromImage(full);
                    UnloadImage(full);
                }
            }
            state->images[i].loaded = true;
            loadedOne = true;
        }

        if (state->images[i].loaded) {
            DrawRectangle(x, y, 128, 128, LIGHTGRAY);
            float scale = fminf((float)128 / state->images[i].texture.width, (float)128 / state->images[i].texture.height);
            float w = state->images[i].texture.width * scale;
            float h = state->images[i].texture.height * scale;
            float tx = x + (128 - w) / 2;
            float ty = y + (128 - h) / 2;
            DrawTexturePro(state->images[i].texture, (Rectangle){0,0, (float)state->images[i].texture.width, (float)state->images[i].texture.height}, (Rectangle){tx, ty, w, h}, (Vector2){0,0}, 0, WHITE);
            DrawRectangleLinesEx((Rectangle){(float)x, (float)y, 128, 128}, 3, state->images[i].selected ? BLUE : GRAY);

            if (state->images[i].selected && state->images[i].selectionOrder > 0) {
                char orderStr[4];
                snprintf(orderStr, 4, "%d", state->images[i].selectionOrder);
                DrawRectangle(x + 4, y + 4, 24, 24, BLUE);
                DrawTextEx(state->font, orderStr, (Vector2){x + 10, y + 8}, 20, 1, WHITE);
            }

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                Rectangle r = {(float)x, (float)y, (float)128, (float)128};
                if (CheckCollisionPointRec(mouse, r)) {
                    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) {
                        state->images[i].selected = !state->images[i].selected;
                        if (state->images[i].selected) {
                            int maxOrder = 0;
                            for (int j = 0; j < state->imageCount; ++j) {
                                if (state->images[j].selectionOrder > maxOrder) maxOrder = state->images[j].selectionOrder;
                            }
                            state->images[i].selectionOrder = maxOrder + 1;
                        } else {
                            int deselectedOrder = state->images[i].selectionOrder;
                            state->images[i].selectionOrder = -1;
                            for (int j = 0; j < state->imageCount; ++j) {
                                if (state->images[j].selectionOrder > deselectedOrder) {
                                    state->images[j].selectionOrder--;
                                }
                            }
                        }
                    } else {
                        Image img = LoadImage(state->images[i].path);
                        if (img.data != NULL) {
                            state->images[i].fullImage = img;
                            state->images[i].fullLoaded = true;
                            state->images[i].fullTexture = LoadTextureFromImage(img);
                            state->images[i].fullTextureLoaded = true;
                            state->currentState = STATE_FULL_VIEW;
                            state->fullViewIndex = i;
                            PreloadNeighbors(state);
                        }
                    }
                }
            }
        }
    }
    EndScissorMode();
}

void DrawFullScreenView(State* state) {
    if (state->fullViewIndex < 0 || !state->images[state->fullViewIndex].fullTextureLoaded) return;

    ClearBackground(RAYWHITE);

    Rectangle titleBar = { 0, 0, (float)GetScreenWidth(), 50 };
    DrawRectangleRec(titleBar, RAYWHITE);
    DrawLine(0, (int)titleBar.height, GetScreenWidth(), (int)titleBar.height, LIGHTGRAY);

    const char* filename = GetFileName(state->images[state->fullViewIndex].path);
    int textSize = MeasureTextEx(state->font, filename, 20, 1).x;
    DrawTextEx(state->font, filename, (Vector2){(GetScreenWidth() - textSize) / 2, (int)(titleBar.height - 20) / 2}, 20, 1, BLACK);

    float canvasWidthIn = strtof(state->bufCanvasW, NULL);
    float canvasHeightIn = strtof(state->bufCanvasH, NULL);
    float marginTopIn = strtof(state->bufMarginT, NULL);
    float marginBottomIn = strtof(state->bufMarginB, NULL);
    float marginLeftIn = strtof(state->bufMarginL, NULL);
    float marginRightIn = strtof(state->bufMarginR, NULL);

    int canvasPxW = canvasWidthIn * 300;
    int canvasPxH = canvasHeightIn * 300;
    int marginT = marginTopIn * 300;
    int marginB = marginBottomIn * 300;
    int marginL = marginLeftIn * 300;
    int marginR = marginRightIn * 300;

    float controls_width = 200;
    Rectangle contentArea = { controls_width, titleBar.height, GetScreenWidth() - controls_width, GetScreenHeight() - titleBar.height };
    float padding = 50.0f;

    float scale = fminf((contentArea.width - padding) / (float)canvasPxW, (contentArea.height - padding) / (float)canvasPxH);
    float dispW = canvasPxW * scale;
    float dispH = canvasPxH * scale;

    float cx = contentArea.x + (contentArea.width - dispW) / 2.0f;
    float cy = contentArea.y + (contentArea.height - dispH) / 2.0f;

    DrawRectangle(cx, cy, dispW, dispH, WHITE);
    DrawRectangleLinesEx((Rectangle){cx, cy, dispW, dispH}, 3, GRAY);

    float marginL_s = marginL * scale;
    float marginR_s = marginR * scale;
    float marginT_s = marginT * scale;
    float marginB_s = marginB * scale;
    float drawW_s = dispW - marginL_s - marginR_s;
    float drawH_s = dispH - marginT_s - marginB_s;

    float imgW = state->images[state->fullViewIndex].fullImage.width;
    float imgH = state->images[state->fullViewIndex].fullImage.height;
    float drawAR = drawW_s / drawH_s;
    float imgAR = imgW / imgH;

    Rectangle src;
    if (imgAR > drawAR) {
        float cropW = imgH * drawAR;
        src = (Rectangle){ (imgW - cropW) / 2.0f, 0, cropW, imgH };
    } else {
        float cropH = imgW / drawAR;
        src = (Rectangle){ 0, (imgH - cropH) / 2.0f, imgW, cropH };
    }

    Rectangle dst = { cx + marginL_s, cy + marginT_s, drawW_s, drawH_s };
    DrawTexturePro(state->images[state->fullViewIndex].fullTexture, src, dst, (Vector2){0, 0}, 0, WHITE);

    int total_controls_height = 140;
    int baseX = 20, baseY = (GetScreenHeight() - total_controls_height) / 2, spacing = 40, inputW = 50, inputH = 25;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state->activeBox = TEXTBOX_NONE;
    }

    DrawTextEx(state->font, "Canvas (in)", (Vector2){baseX, baseY}, 18, 1, GRAY);
    Rectangle rCW = { (float)baseX, (float)baseY + 20, (float)inputW, (float)inputH };
    if (GuiTextBox(rCW, state->bufCanvasW, 8, state->activeBox == TEXTBOX_CANVAS_W)) state->activeBox = TEXTBOX_CANVAS_W;
    DrawTextEx(state->font, "W", (Vector2){baseX + inputW + 5, baseY + 25}, 16, 1, LIGHTGRAY);
    Rectangle rCH = { (float)baseX + inputW + spacing, (float)baseY + 20, (float)inputW, (float)inputH };
    if (GuiTextBox(rCH, state->bufCanvasH, 8, state->activeBox == TEXTBOX_CANVAS_H)) state->activeBox = TEXTBOX_CANVAS_H;
    DrawTextEx(state->font, "H", (Vector2){baseX + inputW + spacing + inputW + 5, baseY + 25}, 16, 1, LIGHTGRAY);

    baseY += 60;
    DrawTextEx(state->font, "Margins (in)", (Vector2){baseX, baseY}, 18, 1, GRAY);
    Rectangle rMT = { (float)baseX, (float)baseY + 20, (float)inputW, (float)inputH };
    if (GuiTextBox(rMT, state->bufMarginT, 8, state->activeBox == TEXTBOX_MARGIN_T)) state->activeBox = TEXTBOX_MARGIN_T;
    DrawTextEx(state->font, "T", (Vector2){baseX + inputW + 5, baseY + 25}, 16, 1, LIGHTGRAY);
    Rectangle rMB = { (float)baseX + inputW + spacing, (float)baseY + 20, (float)inputW, (float)inputH };
    if (GuiTextBox(rMB, state->bufMarginB, 8, state->activeBox == TEXTBOX_MARGIN_B)) state->activeBox = TEXTBOX_MARGIN_B;
    DrawTextEx(state->font, "B", (Vector2){baseX + inputW + spacing + inputW + 5, baseY + 25}, 16, 1, LIGHTGRAY);
    Rectangle rML = { (float)baseX, (float)baseY + 20 + inputH + 10, (float)inputW, (float)inputH };
    if (GuiTextBox(rML, state->bufMarginL, 8, state->activeBox == TEXTBOX_MARGIN_L)) state->activeBox = TEXTBOX_MARGIN_L;
    DrawTextEx(state->font, "L", (Vector2){baseX + inputW + 5, baseY + 25 + inputH + 10}, 16, 1, LIGHTGRAY);
    Rectangle rMR = { (float)baseX + inputW + spacing, (float)baseY + 20 + inputH + 10, (float)inputW, (float)inputH };
    if (GuiTextBox(rMR, state->bufMarginR, 8, state->activeBox == TEXTBOX_MARGIN_R)) state->activeBox = TEXTBOX_MARGIN_R;
    DrawTextEx(state->font, "R", (Vector2){baseX + inputW + spacing + inputW + 5, baseY + 25 + inputH + 10}, 16, 1, LIGHTGRAY);

    if (state->images[state->fullViewIndex].selected) {
        DrawTextEx(state->font, "Selected", (Vector2){GetScreenWidth() - 120, GetScreenHeight() - 40}, 20, 1, BLUE);
    }

    if (IsKeyPressed(KEY_S)) {
        state->images[state->fullViewIndex].selected = !state->images[state->fullViewIndex].selected;
        if (!state->images[state->fullViewIndex].selected) {
            state->images[state->fullViewIndex].selectionOrder = -1;
        } else {
            int maxOrder = 0;
            for (int i = 0; i < state->imageCount; ++i) {
                if (state->images[i].selectionOrder > maxOrder) {
                    maxOrder = state->images[i].selectionOrder;
                }
            }
            state->images[state->fullViewIndex].selectionOrder = maxOrder + 1;
        }
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_L)) {
        int newIndex = (state->fullViewIndex + 1) % state->imageCount;
        UnloadTexture(state->images[state->fullViewIndex].fullTexture);
        UnloadImage(state->images[state->fullViewIndex].fullImage);
        state->images[state->fullViewIndex].fullTextureLoaded = false;
        state->images[state->fullViewIndex].fullLoaded = false;
        state->fullViewIndex = newIndex;
        PreloadNeighbors(state);
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_H)) {
        int newIndex = (state->fullViewIndex - 1 + state->imageCount) % state->imageCount;
        UnloadTexture(state->images[state->fullViewIndex].fullTexture);
        UnloadImage(state->images[state->fullViewIndex].fullImage);
        state->images[state->fullViewIndex].fullTextureLoaded = false;
        state->images[state->fullViewIndex].fullLoaded = false;
        state->fullViewIndex = newIndex;
        PreloadNeighbors(state);
    }

    if (IsKeyPressed(KEY_ESCAPE) || GuiButton((Rectangle){ (float)GetScreenWidth() - 120, (titleBar.height - 30) / 2, 100, 30 }, "Back")) {
        state->selectedFileCount = 0;
        for (int i = 0; i < state->imageCount; ++i) {
            if (state->images[i].selected) {
                state->selectedFileCount++;
            }
        }
        SaveSettings(state);
        
        for (int idx = 0; idx < state->imageCount; idx++) {
            if (state->images[idx].fullTextureLoaded) UnloadTexture(state->images[idx].fullTexture);
            if (state->images[idx].fullLoaded) UnloadImage(state->images[idx].fullImage);
            state->images[idx].fullTextureLoaded = false;
            state->images[idx].fullLoaded = false;
        }
        state->currentState = STATE_GALLERY;
    }
}

void DrawReorderView(State* state) {
    Rectangle titleBar = { 0, 0, (float)GetScreenWidth(), 50 };
    DrawRectangleRec(titleBar, RAYWHITE);
    DrawLine(0, (int)titleBar.height, GetScreenWidth(), (int)titleBar.height, LIGHTGRAY);
    DrawTextEx(state->font, "Reorder/Export", (Vector2){(GetScreenWidth() - MeasureTextEx(state->font, "Reorder/Export", 20, 1).x) / 2, (titleBar.height - 20) / 2}, 20, 1, BLACK);

    if (GuiButton((Rectangle){ 10, (titleBar.height - 30) / 2, 100, 30 }, "Back")) {
        state->currentState = STATE_GALLERY;
    }

    if (GuiButton((Rectangle){ 120, (titleBar.height - 30) / 2, 140, 30 }, "Add Blank Page")) {
        if (state->imageCount < MAX_IMAGES) {
            ImageEntry* blank = &state->images[state->imageCount++];
            strncpy(blank->path, "[BLANK_PAGE]", MAX_PATH_LEN - 1);
            blank->selected = true;
            blank->loaded = true;
            int maxOrder = 0;
            for (int i = 0; i < state->imageCount; ++i) {
                if (state->images[i].selectionOrder > maxOrder) {
                    maxOrder = state->images[i].selectionOrder;
                }
            }
            blank->selectionOrder = maxOrder + 1;
        }
    }

    ImageEntry* sortedSelection[MAX_SELECTED_FILES];
    int selectedCount = 0;
    for (int i = 0; i < state->imageCount; i++) {
        if (state->images[i].selected) {
            sortedSelection[selectedCount++] = &state->images[i];
        }
    }

    for (int i = 0; i < selectedCount - 1; i++) {
        for (int j = 0; j < selectedCount - i - 1; j++) {
            if (sortedSelection[j]->selectionOrder > sortedSelection[j+1]->selectionOrder) {
                ImageEntry* temp = sortedSelection[j];
                sortedSelection[j] = sortedSelection[j+1];
                sortedSelection[j+1] = temp;
            }
        }
    }

    int itemHeight = 40;
    int listWidth = 400;
    int startX = (GetScreenWidth() - listWidth) / 2;
    int startY = (int)titleBar.height + 16;

    for (int i = 0; i < selectedCount; i++) {
        int y = startY + i * itemHeight;
        const char* displayName = strcmp(sortedSelection[i]->path, "[BLANK_PAGE]") == 0 ? "[BLANK_PAGE]" : GetFileName(sortedSelection[i]->path);
        DrawTextEx(state->font, displayName, (Vector2){startX, y + 10}, 20, 1, BLACK);

        if (GuiButton((Rectangle){ (float)startX + listWidth + 10, (float)y, 80, (float)itemHeight - 5 }, "Up")) {
            if (i > 0) {
                int currentOrder = sortedSelection[i]->selectionOrder;
                sortedSelection[i]->selectionOrder = sortedSelection[i-1]->selectionOrder;
                sortedSelection[i-1]->selectionOrder = currentOrder;
            }
        }
        if (GuiButton((Rectangle){ (float)startX + listWidth + 100, (float)y, 80, (float)itemHeight - 5 }, "Down")) {
            if (i < selectedCount - 1) {
                int currentOrder = sortedSelection[i]->selectionOrder;
                sortedSelection[i]->selectionOrder = sortedSelection[i+1]->selectionOrder;
                sortedSelection[i+1]->selectionOrder = currentOrder;
            }
        }
    }

    if (GuiButton((Rectangle){ (float)GetScreenWidth() - 150, (titleBar.height - 30) / 2, 120, 30 }, "Generate PDF")) {
        const char* filterPatterns[] = { "*.pdf" };
        const char* pdfPath = tinyfd_saveFileDialog("Save PDF", "output.pdf", 1, filterPatterns, "PDF Files");

        if (pdfPath && strlen(pdfPath) > 0) {
            float cw = strtof(state->bufCanvasW, NULL);
            float ch = strtof(state->bufCanvasH, NULL);
            float mt = strtof(state->bufMarginT, NULL);
            float mb = strtof(state->bufMarginB, NULL);
            float ml = strtof(state->bufMarginL, NULL);
            float mr = strtof(state->bufMarginR, NULL);

            float pageW = cw * 72.0f;
            float pageH = ch * 72.0f;

            struct pdf_info info = { .creator = "Raylib Viewer", .producer = "PDFGen", .title = "Image Compilation" };
            struct pdf_doc *pdf = pdf_create(pageW, pageH, &info);
            pdf_set_font(pdf, "Helvetica");

            float drawX = ml * 72.0f;
            float drawY = mb * 72.0f;
            float drawW = (cw - ml - mr) * 72.0f;
            float drawH = (ch - mt - mb) * 72.0f;

            for (int i = 0; i < selectedCount; i++) {
                pdf_append_page(pdf);
                if (strcmp(sortedSelection[i]->path, "[BLANK_PAGE]") != 0) {
                    Image img = LoadImage(sortedSelection[i]->path);
                    if (img.data) {
                        float imgW = img.width;
                        float imgH = img.height;
                        float scale = fminf(drawW / imgW, drawH / imgH);
                        float finalW = imgW * scale;
                        float finalH = imgH * scale;
                        float finalX = drawX + (drawW - finalW) / 2.0f;
                        float finalY = drawY + (drawH - finalH) / 2.0f;
                        pdf_add_image_file(pdf, NULL, finalX, finalY, finalW, finalH, sortedSelection[i]->path);
                        UnloadImage(img);
                    }
                }
            }
            pdf_save(pdf, pdfPath);
            pdf_destroy(pdf);
        }
    }
}
