#pragma once

// Deadfrog lib headers.
#include "fonts/df_prop.h"
#include "df_bitmap.h"
#include "df_time.h"
#include "df_window.h"

// Standard headers.
#include <math.h>
#include <string.h>


DfColour g_backgroundColour = { 0xff494949 };
DfColour g_frameColour = { 0xff555555 };
DfColour g_buttonShadowColour = { 0xff323232 };
DfColour g_buttonHighlightColour = { 0xff6f6f6f };
DfColour g_normalTextColour = Colour(210, 210, 210, 255);
DfColour g_selectionColour = Colour(21, 79, 255);

double g_drawScale = 1.0;


// ****************************************************************************
// Misc functions
// ****************************************************************************

int IsMouseInBounds(DfWindow *win, int x, int y, int w, int h) {
    return (win->input.mouseX >= x && win->input.mouseX < (x + w) &&
        win->input.mouseY >= y && win->input.mouseY < (y + h));
}


void HandleDrawScaleChange(DfWindow *win) {
    int scaleChanged = 0;
    if (win->input.keys[KEY_CONTROL]) {
        if (win->input.keyDowns[KEY_EQUALS] || win->input.keyDowns[KEY_PLUS_PAD]) {
            g_drawScale *= 1.1;
            scaleChanged = 1;
        }
        else if (win->input.keyDowns[KEY_MINUS] || win->input.keyDowns[KEY_MINUS_PAD]) {
            g_drawScale /= 1.1;
            scaleChanged = 1;
        }

    }

    if (!g_defaultFont || scaleChanged) {
        g_drawScale = ClampDouble(g_drawScale, 0.7, 3.0);
        double desired_height = g_drawScale * 13.0;
        int bestFontIdx = 0;
        char bestDeltaSoFar = fabs(df_prop.pixelHeights[0] - desired_height);
        for (int i = 1; i < df_prop.numSizes; i++) {
            double delta = fabs(desired_height - df_prop.pixelHeights[i]);
            if (delta < bestDeltaSoFar) {
                bestDeltaSoFar = delta;
                bestFontIdx = i;
            }
        }

        g_defaultFont = LoadFontFromMemory(df_prop.dataBlobs[bestFontIdx], 
            df_prop.dataBlobsSizes[bestFontIdx]);
    }
}


void draw_sunken_box(DfBitmap *bmp, int x, int y, int w, int h) {
    // The specified w and h is the external size of the box.
    //
    //        <-------- w -------->
    //     ^  1 1 1 1 1 1 1 1 1 1 1  ^
    //     |  1 1 1 1 1 1 1 1 1 1 1  | thickness
    //     |  1 1 1 1 1 1 1 1 1 1 1  v
    //     |  3 3 3           4 4 4
    //   h |  3 3 3           4 4 4
    //     |  3 3 3           4 4 4
    //     |  2 2 2 2 2 2 2 2 2 2 2
    //     |  2 2 2 2 2 2 2 2 2 2 2
    //     v  2 2 2 2 2 2 2 2 2 2 2
    //       <--->
    //     thickness

    int thickness = RoundToInt(g_drawScale * 1.5);
    DfColour dark = g_buttonHighlightColour;
    DfColour light = g_buttonShadowColour;
    RectFill(bmp, x, y, w, thickness, light); // '1' pixels
    RectFill(bmp, x, y + h - thickness, w, thickness, dark); // '2' pixels
    RectFill(bmp, x, y + thickness, thickness, h - 2 * thickness, light);
    RectFill(bmp, x + w - thickness, y + thickness, thickness, h - 2 * thickness, dark);

    x += thickness;
    y += thickness;
    w -= thickness * 2;
    h -= thickness * 2;
    RectFill(bmp, x, y, w, h, g_backgroundColour);
}


// ****************************************************************************
// V Scrollbar
// ****************************************************************************

typedef struct {
    int maximum;
    int current_val;
    int covered_range;
    int speed;
} v_scrollbar_t;

int v_scrollbar_do(DfWindow *win, v_scrollbar_t *vs, int x, int y, int w, int h) {
    vs->current_val += win->input.mouseVelZ * vs->speed;
    vs->current_val = ClampInt(vs->current_val, 0, vs->maximum);

    RectFill(win->bmp, x, y, w, h, g_colourBlack);
    return 0;
}


// ****************************************************************************
// Edit box
// ****************************************************************************

typedef struct {
    char text[128];
    double nextCursorToggleTime;
    char cursorOn;
    int cursorIdx;
} edit_box_t;


// Returns 1 if contents changed.
int edit_box_do(DfWindow *win, edit_box_t *eb, int x, int y, int w, int h) {
    draw_sunken_box(win->bmp, x, y, w, h);
    x += 2 * g_drawScale;
    y += 4 * g_drawScale;
    w -= 4 * g_drawScale;
    h -= 8 * g_drawScale;
    SetClipRect(win->bmp, x, y, w, h);

    double now = GetRealTime();
    if (now > eb->nextCursorToggleTime) {
        eb->cursorOn = !eb->cursorOn;
        eb->nextCursorToggleTime = now + 0.5;
    }

    if (win->input.keyDowns[KEY_LEFT]) {
        eb->cursorIdx = IntMax(0, eb->cursorIdx - 1);
        eb->nextCursorToggleTime = now;
    }
    else if (win->input.keyDowns[KEY_RIGHT]) {
        eb->cursorIdx = IntMin(strlen(eb->text), eb->cursorIdx + 1);
        eb->nextCursorToggleTime = now;
    }
    else if (win->input.keyDowns[KEY_HOME]) {
        eb->cursorIdx = 0;
        eb->nextCursorToggleTime = now;
    }
    else if (win->input.keyDowns[KEY_END]) {
        eb->cursorIdx = strlen(eb->text);
        eb->nextCursorToggleTime = now;
    }

    int contents_changed = 0;
    for (int i = 0; i < win->input.numKeysTyped; i++) {
        char c = win->input.keysTyped[i];
        if (c == 8) { // Backspace
            if (eb->cursorIdx > 0) {
                char *move_src = eb->text + eb->cursorIdx;
                unsigned move_size = strlen(move_src);
                memmove(move_src - 1, move_src, move_size);
                move_src[move_size - 1] = '\0';
                eb->cursorIdx--;
            }
        }
        else if (c == 127) { // Delete
            char *move_src = eb->text + eb->cursorIdx + 1;
            unsigned move_size = strlen(move_src);
            memmove(move_src - 1, move_src, move_size);
            move_src[move_size - 1] = '\0';
        }
        else {
            char *move_src = eb->text + eb->cursorIdx;
            unsigned move_size = strlen(move_src);
            memmove(move_src + 1, move_src, move_size);
            eb->text[eb->cursorIdx] = c;
            eb->text[sizeof(eb->text) - 1] = '\0';
            eb->cursorIdx = IntMin(eb->cursorIdx + 1, sizeof(eb->text) - 1);
        }

        contents_changed = 1;
        eb->nextCursorToggleTime = now;
    }

    DrawTextSimple(g_defaultFont, g_normalTextColour, win->bmp, x, y, eb->text);

    if (eb->cursorOn) {
        int cursorX = GetTextWidth(g_defaultFont, eb->text, eb->cursorIdx) + x;
        RectFill(win->bmp, cursorX, y, 2 * g_drawScale, g_defaultFont->charHeight, g_normalTextColour);
    }
    
    ClearClipRect(win->bmp);

    return contents_changed;
}


// ****************************************************************************
// List View
// ****************************************************************************

typedef struct {
    char const **items;
    int num_items;
    int selected_item;
    int first_display_item;
} list_view_t;


// Returns id of item that was selected, or -1 if none were.
int list_view_do(DfWindow *win, list_view_t *lv, int x, int y, int w, int h) {
    draw_sunken_box(win->bmp, x, y, w, h);
    x += 2 * g_drawScale;
    y += 2 * g_drawScale;
    w -= 4 * g_drawScale;
    h -= 4 * g_drawScale;
    SetClipRect(win->bmp, x, y, w, h);

    const int num_rows = RoundToInt(h / (double)g_defaultFont->charHeight - 0.9);

    if (win->input.keyDowns[KEY_DOWN]) {
        lv->selected_item++;
        lv->first_display_item = IntMax(lv->selected_item - num_rows, lv->first_display_item);
    }
    else if (win->input.keyDowns[KEY_UP]) {
        lv->selected_item--;
        lv->first_display_item = IntMin(lv->selected_item, lv->first_display_item);
    }
    else if (win->input.keyDowns[KEY_PGDN]) {
        lv->selected_item += num_rows;
        lv->first_display_item += num_rows;
    }
    else if (win->input.keyDowns[KEY_PGUP]) {
        lv->selected_item -= num_rows;
        lv->first_display_item -= num_rows;
    }

    if (win->input.lmbClicked && IsMouseInBounds(win, x, y, w, h)) {
        int row = (win->input.mouseY - y) / g_defaultFont->charHeight;
        lv->selected_item = row + lv->first_display_item;
    }

    if (lv->selected_item >= lv->num_items || lv->num_items <= 0)
        lv->selected_item = lv->num_items - 1;
    else if (lv->selected_item < 0)
        lv->selected_item = 0;

    lv->first_display_item -= win->input.mouseVelZ / 36;
    lv->first_display_item = ClampInt(lv->first_display_item, 0, lv->num_items - num_rows);
    if (lv->num_items <= num_rows) lv->first_display_item = 0;

    int last_y = y + h;
    for (int i = lv->first_display_item; i < lv->num_items; i++) {
        if (y > last_y) break;

        if (i == lv->selected_item) {
            RectFill(win->bmp, x, y, w, g_defaultFont->charHeight, g_selectionColour);
        }

        DrawTextSimple(g_defaultFont, g_normalTextColour, win->bmp, 
                       x + 2 * g_drawScale, y, lv->items[i]);
        y += g_defaultFont->charHeight;
    }

    ClearClipRect(win->bmp);

    return -1;
}


// ****************************************************************************
// Text View
// ****************************************************************************

enum { TEXT_VIEW_MAX_CHARS = 95000 };

typedef struct {
    char text[TEXT_VIEW_MAX_CHARS];
} text_view_t;


void text_view_init(text_view_t *tv) {
    tv->text[0] = '\0';
}


void text_view_empty(text_view_t *tv) {
    text_view_init(tv);
}


void text_view_add_text(text_view_t *tv, char const *text) {
    int current_len = strlen(tv->text);
    int additional_len = strlen(text);
    int space = TEXT_VIEW_MAX_CHARS - current_len - 1;
    int amt_to_copy = IntMin(space, additional_len);
    memcpy(tv->text + current_len, text, amt_to_copy);
    tv->text[current_len + amt_to_copy] = '\0';
}


char const *find_space(char const *c) {
    while (*c != ' ' && *c != '\n' && *c != '\0') 
        c++;
    return c;
}


void text_view_do(DfWindow *win, text_view_t *tv, int x, int y, int w, int h) {
    draw_sunken_box(win->bmp, x, y, w, h);
    x += 4 * g_drawScale;
    y += 2 * g_drawScale;
    w -= 8 * g_drawScale;
    h -= 4 * g_drawScale;
    SetClipRect(win->bmp, x, y, w, h);

    int space_pixels = GetTextWidth(g_defaultFont, " ");
    char const *c = tv->text;
    int current_x = x;
    while (1) {
        char const *space = find_space(c);
        unsigned word_len = space - c;

        int num_pixels = GetTextWidth(g_defaultFont, c, word_len);
        if (*space != '\n' && current_x + num_pixels >= win->bmp->clipRight) {
            current_x = x;
            y += g_defaultFont->charHeight;
        }

        DrawTextSimpleLen(g_defaultFont, g_normalTextColour, win->bmp, 
            current_x, y, c, word_len);

        current_x += num_pixels + space_pixels;
        c += word_len;

        if (*space == '\n') {
            current_x = x;
            y += g_defaultFont->charHeight;
        }

        if (*c == '\0') break;
        c++;
    }

    ClearClipRect(win->bmp);
}
