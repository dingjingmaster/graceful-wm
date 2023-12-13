//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_TREE_H
#define GRACEFUL_WM_TREE_H
#include <xcb/xcb.h>

#include "types.h"


void tree_render (void);
bool tree_level_up (void);
bool tree_level_down (void);
void tree_flatten (GWMContainer* child);
void tree_init (xcb_get_geometry_reply_t* geo);
bool tree_next (GWMContainer* con, GWMDirection direction);
void tree_split (GWMContainer* con, GWMOrientation orientation);
bool tree_restore (const char* path, xcb_get_geometry_reply_t* geometry);
GWMContainer* tree_open_container (GWMContainer* con, GWMWindow* window);
GWMContainer* tree_get_tree_next_sibling (GWMContainer* con, GWMPosition direction);
void tree_append_json(GWMContainer* con, const char *buf, size_t len, char** errorMsg);
bool tree_close_internal (GWMContainer* con, GWMKillWindow killWindow, bool doNotKillParent);

#endif //GRACEFUL_WM_TREE_H
