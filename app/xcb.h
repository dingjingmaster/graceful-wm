//
// Created by dingjing on 23-11-27.
//

#ifndef GRACEFUL_WM_XCB_H
#define GRACEFUL_WM_XCB_H
#include "types.h"

#define _NET_WM_STATE_REMOVE                0
#define _NET_WM_STATE_ADD                   1
#define _NET_WM_STATE_TOGGLE                2

#define XCB_NUM_LOCK                        0xff7f          // from X11/keysymdef.h
#define CHILD_EVENT_MASK                    \
    (XCB_EVENT_MASK_PROPERTY_CHANGE         \
    | XCB_EVENT_MASK_STRUCTURE_NOTIFY       \
    | XCB_EVENT_MASK_FOCUS_CHANGE)

#define FRAME_EVENT_MASK                    \
    (XCB_EVENT_MASK_BUTTON_PRESS            \
    | XCB_EVENT_MASK_BUTTON_RELEASE         \
    | XCB_EVENT_MASK_POINTER_MOTION         \
    | XCB_EVENT_MASK_EXPOSURE               \
    | XCB_EVENT_MASK_STRUCTURE_NOTIFY       \
    | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT  \
    | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY    \
    | XCB_EVENT_MASK_ENTER_WINDOW)

#define ROOT_EVENT_MASK                     \
    (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT   \
    | XCB_EVENT_MASK_BUTTON_PRESS           \
    | XCB_EVENT_MASK_STRUCTURE_NOTIFY       \
    | XCB_EVENT_MASK_POINTER_MOTION         \
    | XCB_EVENT_MASK_PROPERTY_CHANGE        \
    | XCB_EVENT_MASK_FOCUS_CHANGE           \
    | XCB_EVENT_MASK_ENTER_WINDOW)

# include "xmacro-atoms_NET-SUPPORTED.h"
#define GWM_ATOM_MACRO(atom) extern xcb_atom_t A_##atom;
GWM_NET_SUPPORTED_ATOMS_XMACRO
#undef GWM_ATOM_MACRO

#endif //GRACEFUL_WM_XCB_H
