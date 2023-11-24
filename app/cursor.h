//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_CURSOR_H
#define GRACEFUL_WM_CURSOR_H
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>

typedef enum Cursor
{
    CURSOR_POINTER = 0,
    CURSOR_RESIZE_HORIZONTAL,
    CURSOR_RESIZE_VERTICAL,
    CURSOR_TOP_LEFT_CORNER,
    CURSOR_TOP_RIGHT_CORNER,
    CURSOR_BUTTON_LEFT_CORNER,
    CURSOR_BUTTON_RIGHT_CORNER,
    CURSOR_BUTTON_WATCH,
    CURSOR_BUTTON_MOVE,
    CURSOR_BUTTON_MAX,
} Cursor;


void            cursor_load_cursor(void);
xcb_cursor_t    cursor_get_cursor(Cursor cursor);
void            cursor_set_root_cursor(int cursorID);


#endif //GRACEFUL_WM_CURSOR_H
