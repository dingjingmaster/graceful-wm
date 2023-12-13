//
// Created by dingjing on 23-11-24.
//

#include "cursor.h"

#include <glib/gi18n.h>
#include <xcb/xcb_cursor.h>

#include "val.h"
#include "log.h"

static xcb_cursor_context_t*        gCtx = NULL;
static xcb_cursor_t                 gCursors[CURSOR_MAX];


void cursor_load_cursor(void)
{
    if (xcb_cursor_context_new (gConn, gRootScreen, &gCtx) < 0) {
        ERROR(_("Cannot allocate cursor context"));
        exit (-1);
    }

#define LOAD_CURSOR(constant, name)                                 \
    do {                                                            \
        gCursors[constant] = xcb_cursor_load_cursor(gCtx, name);    \
    } while (0)
    LOAD_CURSOR(CURSOR_WATCH,               "watch");
    LOAD_CURSOR(CURSOR_MOVE,                "fleur");
    LOAD_CURSOR(CURSOR_POINTER,             "left_ptr");
    LOAD_CURSOR(CURSOR_TOP_LEFT_CORNER,     "top_left_corner");
    LOAD_CURSOR(CURSOR_TOP_RIGHT_CORNER,    "top_right_corner");
    LOAD_CURSOR(CURSOR_RESIZE_HORIZONTAL,   "sb_h_double_arrow");
    LOAD_CURSOR(CURSOR_RESIZE_VERTICAL,     "sb_v_double_arrow");
    LOAD_CURSOR(CURSOR_BOTTOM_LEFT_CORNER,  "bottom_left_corner");
    LOAD_CURSOR(CURSOR_BOTTOM_RIGHT_CORNER, "bottom_right_corner");
#undef LOAD_CURSOR
}

xcb_cursor_t cursor_get_cursor(GWMCursor cursor)
{
    g_return_val_if_fail(cursor >= CURSOR_POINTER && cursor < CURSOR_MAX, CURSOR_POINTER);

    return gCursors[cursor];
}

void cursor_set_root_cursor(int cursorID)
{
    xcb_change_window_attributes(gConn, gRoot, XCB_CW_CURSOR, (uint32_t[]) { cursor_get_cursor(cursorID) });
}
