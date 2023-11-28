//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_CURSOR_H
#define GRACEFUL_WM_CURSOR_H
#include "types.h"

#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>


void            cursor_load_cursor(void);
xcb_cursor_t    cursor_get_cursor(GWMCursor cursor);
void            cursor_set_root_cursor(int cursorID);


#endif //GRACEFUL_WM_CURSOR_H
