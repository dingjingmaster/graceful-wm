//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_TREE_H
#define GRACEFUL_WM_TREE_H
#include <xcb/xcb.h>


void tree_render (void);
void tree_init (xcb_get_geometry_reply_t* geo);

#endif //GRACEFUL_WM_TREE_H
