//
// Created by dingjing on 23-11-27.
//

#include "xcb.h"

xcb_visualid_t xcb_gwm_get_visualid_by_depth(uint16_t depth)
{
    return 0;
}

void xcb_gwm_fake_absolute_configure_notify(GWMContainer *con)
{

}

xcb_visibility_t *xcb_gwm_get_visual_type_by_id(xcb_visualid_t visualID)
{
    return NULL;
}

void xcb_gwm_send_take_focus(xcb_window_t window, xcb_timestamp_t timeStamp)
{

}

xcb_atom_t xcb_gwm_get_preferred_window_type(xcb_get_property_reply_t *reply)
{
    return 0;
}

bool xcb_gwm_reply_contains_atom(xcb_get_property_reply_t *prop, xcb_atom_t atom)
{
    return 0;
}

void xcb_gwm_grab_buttons(xcb_connection_t *conn, xcb_window_t window, int *buttons)
{

}

void xcb_gwm_set_window_rect(xcb_connection_t *conn, xcb_window_t window, GWMRect r)
{

}

void xcb_gwm_add_property_atom(xcb_connection_t *conn, xcb_window_t window, xcb_atom_t property, xcb_atom_t atom)
{

}

void xcb_gwm_remove_property_atom(xcb_connection_t *conn, xcb_window_t window, xcb_atom_t property, xcb_atom_t atom)
{

}

xcb_window_t xcb_gwm_create_window(xcb_connection_t *conn, GWMRect r, uint16_t depth, xcb_visualid_t visual, uint16_t windowClass, GWMCursor cursor, bool map, uint32_t mask, uint32_t *values)
{
    return 0;
}
