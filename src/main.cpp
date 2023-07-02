// Deadfrog lib headers
#include "df_font.h"
#include "df_window.h"

// Project headers
#include "gui.h"

// Standard headers
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

static const char APPLICATION_NAME[] = "Shrinky McShrinkface";

DfWindow *g_win;


static void draw_frame() {
    HandleDrawScaleChange(g_win);

    UpdateWin(g_win);
}


typedef struct {
    char const *name;
    int num_bytes;
} Function;

typedef struct {
    char const *name;
    int type_id;
} DataObject;

typedef struct {
    char const *moduleName;
    Function *functions;
    int numFunctions;
    DataObject *dataObjects;
    int numDataObjects;
} Module;


static char *g_dumpFile = NULL;
static int g_dumpFileNumBytes = 0;
static int g_numModules = 0;
static Module *g_modules = NULL;


static int StartsWith(char const *haystack, char const *needle) {
    while (*needle != '\0') {
        if (*haystack != *needle)
            return 0;
        haystack++;
        needle++;
    }

    return 1;
}


static int GetNextModuleOffset(int startOffset) {
    for (int i = startOffset; i < g_dumpFileNumBytes; i++) {
        if (StartsWith(g_dumpFile + i, "\n** Module: ")) {
            return i;
        }
    }

    return -1;
}


static char const *DuplicateStringUptoChar(char const *s, char c) {
    int len = 0;
    while (s[len] != c) len++;

    char *newS = (char *)malloc(len + 1);
    memcpy(newS, s, len);
    newS[len] = '\0';

    return newS;
}


static int CountModules(int offset) {
    offset++;
    int count = 0;
    for (int i = offset; i < g_dumpFileNumBytes; i++) {
        char const *c = g_dumpFile + i;
        if (StartsWith(c, "\n** Module: "))
            count++;

        if (StartsWith(c, "\n*** "))
            break;
    }

    return count;
}


static int g_totalCodeSize = 0;
static int g_totalNumFunctions = 0;


static int ParseModule(Module *mod, int offset) {
    offset += 13;
    mod->moduleName = DuplicateStringUptoChar(g_dumpFile + offset, '"');
    offset += 3;

    if (strcmp(mod->moduleName, "Import:MSVCR120.dll") == 0)
        mod = mod;

    // Count functions and data objects in this module.
    mod->numFunctions = mod->numDataObjects = 0;
    for (int i = offset; i < g_dumpFileNumBytes; i++) {
        char const *c = g_dumpFile + i;
        if (StartsWith(c, "\n** Module: ") || StartsWith(c, "\n*** GLOBALS\n"))
            break;

        if (StartsWith(c, "\n(") && StartsWith(c + 8, ") S_")) {
            c += 13;
            if (StartsWith(c, "PROC32: ") || StartsWith(c, "HUNK32: ")) {
                mod->numFunctions++;
            }
            else if (StartsWith(c, "DATA32: ")) {
                mod->numDataObjects++;
            }
        }
    }

    if (mod->numFunctions)
        mod->functions = (Function *)calloc(mod->numFunctions, sizeof(Function));
    if (mod->numDataObjects)
        mod->dataObjects = (DataObject *)calloc(mod->numDataObjects, sizeof(DataObject));

    int numFunctions = 0;
    int numDataObjects = 0;
    for (int i = offset; i < g_dumpFileNumBytes; i++) {
        char const *c = g_dumpFile + i;
        if (StartsWith(c, "\n** Module: ") || StartsWith(c, "\n*** GLOBALS\n"))
            return i;

        if (StartsWith(c, "\n(") && StartsWith(c + 8, ") S_")) {
            c += 13;
            if (StartsWith(c, "PROC32: ") || StartsWith(c, "HUNK32: ")) {
                int isThunk = 0;
                if (*c == 'H')
                    isThunk = 1;

                Function *f = &mod->functions[numFunctions];
                c = strstr(c, "Cb: ") + 4;
                f->num_bytes = strtol(c, NULL, 16);
                g_totalCodeSize += f->num_bytes;
                g_totalNumFunctions++;

                if (!isThunk)
                    c = strstr(c, "Type: ") + 6;
                c = strstr(c, ", ") + 2;
                f->name = DuplicateStringUptoChar(c, '\n');

                numFunctions++;
            }
            else if (StartsWith(c, "DATA32: ") || StartsWith(c, "THREAD32: ")) {
                DataObject *dob = &mod->dataObjects[numDataObjects];

                c = strstr(c, "Type:") + 5;
                while (*c == ' ') c++;
                if (StartsWith(c, "0x"))
                    dob->type_id = strtol(c + 3, NULL, 16);
                else if (StartsWith(c, "T_"))
                    dob->type_id = -1;

                c += 3;
                char const *name = strstr(c, ", ");
                dob->name = DuplicateStringUptoChar(name + 2, '\n');

                numDataObjects++;
            }
        }
    }

    return g_dumpFileNumBytes;
}


int parseFile(Module *modSyms, char const *filename) {
    // Read the entire file into memory.
    FILE *in = fopen(filename, "r");
    if (!in) return 0;
    fseek(in, 0, SEEK_END);
    g_dumpFileNumBytes = (int)ftell(in);
    fseek(in, 0, SEEK_SET);
    g_dumpFile = (char *)malloc(g_dumpFileNumBytes);
    int bytesRead = (int)fread(g_dumpFile, 1, g_dumpFileNumBytes, in);
    fclose(in);
    if (bytesRead < g_dumpFileNumBytes/2)
        return 0;

    // Find start of SYMBOLS section.
    int symsStartOffset = -1;
    for (int i = 0; i < g_dumpFileNumBytes; i++) {
        if (StartsWith(g_dumpFile + i, "\n*** SYMBOLS\n")) {
            symsStartOffset = i;
            break;
        }
    }
    ReleaseAssert(symsStartOffset >= 0, "Couldn't find '\n*** SYMBOLS\n' in input file");

    // Parse modules
    g_numModules = CountModules(symsStartOffset);
    g_modules = (Module *)calloc(g_numModules, sizeof(Module));

    int offset = symsStartOffset;
    for (int i = 0; i < g_numModules; i++) {
        int modOffset = GetNextModuleOffset(offset);
        offset = ParseModule(&g_modules[i], modOffset);
    }

    return 1;
}


void main() {
    g_win = CreateWin(800, 600, WT_WINDOWED_RESIZEABLE, APPLICATION_NAME);
    RegisterRedrawCallback(g_win, draw_frame);

    Module modSyms;
    parseFile(&modSyms, "c:/coding/chambers_dict/pdb.dump");


    //
    // Init GUI widgets


    //
    // Main loop

    double next_force_frame_time = GetRealTime() + 0.2;
    while (!g_win->windowClosed && !g_win->input.keyDowns[KEY_ESC]) {
        bool force_frame = GetRealTime() > next_force_frame_time;
        if (force_frame) {
            next_force_frame_time = GetRealTime() + 0.2;
        }

        if (InputPoll(g_win) || force_frame) {
            draw_frame();
        }
         
        WaitVsync();
    }
}


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI WinMain(HINSTANCE _hInstance, HINSTANCE /*_hPrevInstance*/,
    LPSTR cmdLine, int /*_iCmdShow*/)
{
    main();
    return 0;
}
