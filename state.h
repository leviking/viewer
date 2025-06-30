#ifndef STATE_H
#define STATE_H

#include "raylib.h"
#include "pdfgen.h"
#include <stdbool.h>

#define MAX_IMAGES 4096
#define MAX_PATH_LEN 512
#define MAX_FILENAME_LEN 256
#define MAX_SELECTED_FILES 512

typedef struct {
    char path[MAX_PATH_LEN];
    Texture2D texture;
    Image fullImage;
    Texture2D fullTexture;
    bool loaded;
    bool fullLoaded;
    bool fullTextureLoaded;
    bool selected;
    int selectionOrder;
} ImageEntry;

typedef enum {
    STATE_GALLERY,
    STATE_FULL_VIEW,
    STATE_REORDER
} AppState;

typedef enum {
    TEXTBOX_NONE,
    TEXTBOX_CANVAS_W,
    TEXTBOX_CANVAS_H,
    TEXTBOX_MARGIN_T,
    TEXTBOX_MARGIN_B,
    TEXTBOX_MARGIN_L,
    TEXTBOX_MARGIN_R
} ActiveTextBox;

typedef struct {
    ImageEntry images[MAX_IMAGES];
    int imageCount;
    float scrollY;
    AppState currentState;
    int fullViewIndex;
    int prevIndex;
    int nextIndex;
    char folder[MAX_PATH_LEN];
    char bufCanvasW[8];
    char bufCanvasH[8];
    char bufMarginT[8];
    char bufMarginB[8];
    char bufMarginL[8];
    char bufMarginR[8];
    ActiveTextBox activeBox;
    char selectedFiles[MAX_SELECTED_FILES][MAX_FILENAME_LEN];
    int selectedFileCount;
    Font font;
} State;

void LoadFolder(State* state, const char* folderPath);
void PreloadNeighbors(State* state);
void InitializeState(State* state);
bool FileExists(const char *path);
void GetThumbPath(const char *folder, const char *filename, char *outPath);

#endif // STATE_H
