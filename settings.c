#include "settings.h"
#include <stdio.h>
#include <string.h>

void SaveSettings(const State* state) {
    char settingsPath[MAX_PATH_LEN];
    snprintf(settingsPath, MAX_PATH_LEN, "%s/.rayview_settings", state->folder);
    FILE* f = fopen(settingsPath, "w");
    if (f) {
        fprintf(f, "%s\n%s\n%s\n%s\n%s\n%s\n", state->bufCanvasW, state->bufCanvasH, state->bufMarginT, state->bufMarginB, state->bufMarginL, state->bufMarginR);
        for (int i = 0; i < state->selectedFileCount; ++i) {
            fprintf(f, "%s\n", state->selectedFiles[i]);
        }
        fclose(f);
    }
}

void LoadSettings(State* state) {
    char settingsPath[MAX_PATH_LEN];
    snprintf(settingsPath, MAX_PATH_LEN, "%s/.rayview_settings", state->folder);
    FILE* f = fopen(settingsPath, "r");
    if (f) {
        if (fscanf(f, "%s\n%s\n%s\n%s\n%s\n%s\n", state->bufCanvasW, state->bufCanvasH, state->bufMarginT, state->bufMarginB, state->bufMarginL, state->bufMarginR) != 6) {
            // Handle error or incomplete file
        }
        state->selectedFileCount = 0;
        while (state->selectedFileCount < MAX_SELECTED_FILES && fscanf(f, "%s\n", state->selectedFiles[state->selectedFileCount]) != EOF) {
            (state->selectedFileCount)++;
        }
        fclose(f);
    }
}
