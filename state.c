#include "state.h"
#include "settings.h"
#include "tinyfiledialogs.h"
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

bool HasImageExtension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".png") == 0 ||
            strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0);
}

void EnsureDirectoryExists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

void GetThumbPath(const char *folder, const char *filename, char *outPath) {
    char thumbDir[MAX_PATH_LEN];
    snprintf(thumbDir, MAX_PATH_LEN, "%s/.rayview_thumbs", folder);
    EnsureDirectoryExists(thumbDir);
    snprintf(outPath, MAX_PATH_LEN, "%s/%s__thumb.png", thumbDir, filename);
}

bool FileExists(const char *path) {
    return access(path, F_OK) != -1;
}

void LoadFolder(State* state, const char* folderPath) {
    DIR* dir = opendir(folderPath);
    if (!dir) return;

    struct dirent* entry;
    state->imageCount = 0;

    while ((entry = readdir(dir)) != NULL && state->imageCount < MAX_IMAGES) {
        if (!HasImageExtension(entry->d_name)) continue;

        snprintf(state->images[state->imageCount].path, MAX_PATH_LEN, "%s/%s", folderPath, entry->d_name);
        state->images[state->imageCount].loaded = false;
        state->images[state->imageCount].fullLoaded = false;
        state->images[state->imageCount].fullTextureLoaded = false;
        state->images[state->imageCount].selected = false;
        state->images[state->imageCount].selectionOrder = -1;
        state->imageCount++;
    }

    closedir(dir);
}

void PreloadNeighbors(State* state) {
    state->prevIndex = (state->fullViewIndex - 1 + state->imageCount) % state->imageCount;
    state->nextIndex = (state->fullViewIndex + 1) % state->imageCount;

    int indices[] = {state->prevIndex, state->nextIndex};
    for (int i = 0; i < 2; ++i) {
        int idx = indices[i];
        if (!state->images[idx].fullLoaded) {
            Image img = LoadImage(state->images[idx].path);
            if (img.data != NULL) {
                state->images[idx].fullImage = img;
                state->images[idx].fullLoaded = true;
                state->images[idx].fullTexture = LoadTextureFromImage(img);
                state->images[idx].fullTextureLoaded = true;
            }
        }
    }
}

void InitializeState(State* state) {
    state->imageCount = 0;
    state->scrollY = 0;
    state->currentState = STATE_GALLERY;
    state->fullViewIndex = -1;
    state->prevIndex = -1;
    state->nextIndex = -1;
    strcpy(state->bufCanvasW, "8.0");
    strcpy(state->bufCanvasH, "10.0");
    strcpy(state->bufMarginT, "1.0");
    strcpy(state->bufMarginB, "1.0");
    strcpy(state->bufMarginL, "1.0");
    strcpy(state->bufMarginR, "1.0");
    state->activeBox = TEXTBOX_NONE;
    state->selectedFileCount = 0;

    const char* initialFolder = tinyfd_selectFolderDialog("Select a folder of images", ".");
    if (!initialFolder || strlen(initialFolder) == 0) {
        exit(0);
    }
    strncpy(state->folder, initialFolder, MAX_PATH_LEN - 1);
    state->folder[MAX_PATH_LEN - 1] = '\0';

    // Load folder first, then apply settings
    LoadFolder(state, state->folder);
    LoadSettings(state);
    state->font = LoadFont("Lato-Regular.ttf");
}
