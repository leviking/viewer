#include "raylib.h"
#include "state.h"
#include "ui.h"
#include "settings.h"
#include <string.h>

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 800, "Graphics Assembler");
    SetTargetFPS(60);

    State state;
    InitializeState(&state);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        switch (state.currentState) {
            case STATE_GALLERY:
                DrawGalleryView(&state);
                break;
            case STATE_FULL_VIEW:
                DrawFullScreenView(&state);
                break;
            case STATE_REORDER:
                DrawReorderView(&state);
                break;
        }

        EndDrawing();
    }

    // Save settings on exit
    state.selectedFileCount = 0;
    ImageEntry* sortedSelection[MAX_SELECTED_FILES];
    for (int i = 0; i < state.imageCount; ++i) {
        if (state.images[i].selected) {
            sortedSelection[state.selectedFileCount++] = &state.images[i];
        }
    }
    for (int i = 0; i < state.selectedFileCount - 1; i++) {
        for (int j = 0; j < state.selectedFileCount - i - 1; j++) {
            if (sortedSelection[j]->selectionOrder > sortedSelection[j+1]->selectionOrder) {
                ImageEntry* temp = sortedSelection[j];
                sortedSelection[j] = sortedSelection[j+1];
                sortedSelection[j+1] = temp;
            }
        }
    }
    for (int i = 0; i < state.selectedFileCount; ++i) {
        strncpy(state.selectedFiles[i], GetFileName(sortedSelection[i]->path), MAX_FILENAME_LEN - 1);
        state.selectedFiles[i][MAX_FILENAME_LEN - 1] = '\0';
    }
    SaveSettings(&state);

    for (int i = 0; i < state.imageCount; i++) {
        if (state.images[i].loaded) UnloadTexture(state.images[i].texture);
        if (state.images[i].fullTextureLoaded) UnloadTexture(state.images[i].fullTexture);
        if (state.images[i].fullLoaded) UnloadImage(state.images[i].fullImage);
    }
    UnloadFont(state.font);

    CloseWindow();
    return 0;
}