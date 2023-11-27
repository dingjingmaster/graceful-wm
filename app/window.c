//
// Created by dingjing on 23-11-27.
//

#include "window.h"

void window_free(GWMWindow *win)
{

}

void window_update_role(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_icon(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_name(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_class(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_leader(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_machine(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_type(GWMWindow *window, xcb_get_property_reply_t *reply)
{

}

void window_update_name_legacy(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_strut_partial(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_transient_for(GWMWindow *win, xcb_get_property_reply_t *prop)
{

}

void window_update_hints(GWMWindow *win, xcb_get_property_reply_t *prop, bool *urgencyHint)
{

}

bool window_update_motif_hints(GWMWindow *win, xcb_get_property_reply_t *prop, GWMBorderStyle *motifBorderStyle)
{
    return 0;
}

bool window_update_normal_hints(GWMWindow *win, xcb_get_property_reply_t *reply, xcb_get_geometry_reply_t *geom)
{
    return 0;
}
