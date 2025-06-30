#include "settings.h"
#include <stdio.h>
#include <string.h>

void SaveSettings(const State* state) {
    char settingsPath[MAX_PATH_LEN];
    snprintf(settingsPath, MAX_PATH_LEN, "%s/.rayview_settings", state->folder);
    FILE* f = fopen(settingsPath, "w");
    if (f) {
        fprintf(f, "%s\n%s\n%s\n%s\n%s\n%s\n", state->bufCanvasW, state->bufCanvasH, state->bufMarginT, state->bufMarginB, state->bufMarginL, state->bufMarginR);
        
        ImageEntry* sortedSelection[MAX_SELECTED_FILES];
        int selectedCount = 0;
        for (int i = 0; i < state->imageCount; i++) {
            if (state->images[i].selected) {
                sortedSelection[selectedCount++] = (ImageEntry*)&state->images[i];
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

        for (int i = 0; i < selectedCount; ++i) {
            fprintf(f, "%s\n", GetFileName(sortedSelection[i]->path));
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
        
        char line[MAX_FILENAME_LEN];
        int order = 1;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = 0; // Remove newline
            if (strcmp(line, "[BLANK_PAGE]") == 0) {
                if (state->imageCount < MAX_IMAGES) {
                    ImageEntry* blank = &state->images[state->imageCount++];
                    strncpy(blank->path, "[BLANK_PAGE]", MAX_PATH_LEN - 1);
                    blank->selected = true;
                    blank->selectionOrder = order++;
                    blank->loaded = true; // Mark as loaded to avoid processing
                }
            } else {
                for (int i = 0; i < state->imageCount; ++i) {
                    if (strcmp(GetFileName(state->images[i].path), line) == 0) {
                        state->images[i].selected = true;
                        state->images[i].selectionOrder = order++;
                        break;
                    }
                }
            }
        }
        fclose(f);
    }
}
