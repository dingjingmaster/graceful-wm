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

#include "xmacro-atoms_reset.h"
#include "xmacro-atoms_NET-SUPPORTED.h"
#define GWM_ATOM_MACRO(atom) extern xcb_atom_t A_##atom;
GWM_NET_SUPPORTED_ATOMS_XMACRO
GWM_REST_ATOMS_XMACRO
#undef GWM_ATOM_MACRO

xcb_visualid_t xcb_gwm_get_visualid_by_depth (uint16_t depth);
void xcb_gwm_fake_absolute_configure_notify (GWMContainer* con);
xcb_visibility_t* xcb_gwm_get_visual_type_by_id (xcb_visualid_t visualID);
void xcb_gwm_send_take_focus (xcb_window_t window, xcb_timestamp_t timeStamp);
xcb_atom_t xcb_gwm_get_preferred_window_type (xcb_get_property_reply_t* reply);
bool xcb_gwm_reply_contains_atom (xcb_get_property_reply_t* prop, xcb_atom_t atom);
void xcb_gwm_grab_buttons (xcb_connection_t* conn, xcb_window_t window, int* buttons);
void xcb_gwm_set_window_rect (xcb_connection_t* conn, xcb_window_t window, GWMRect r);
void xcb_gwm_add_property_atom (xcb_connection_t* conn, xcb_window_t window, xcb_atom_t property, xcb_atom_t atom);
void xcb_gwm_fake_configure_notify(xcb_connection_t *conn, xcb_rectangle_t r, xcb_window_t window, int borderWidth);
void xcb_gwm_remove_property_atom (xcb_connection_t* conn, xcb_window_t window, xcb_atom_t property, xcb_atom_t atom);
xcb_window_t xcb_gwm_create_window (xcb_connection_t *conn, GWMRect r, uint16_t depth, xcb_visualid_t visual, uint16_t windowClass, GWMCursor cursor, bool map, uint32_t mask, uint32_t *values);

#endif //GRACEFUL_WM_XCB_H
