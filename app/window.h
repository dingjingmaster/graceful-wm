//
// Created by dingjing on 23-11-27.
//

#ifndef GRACEFUL_WM_WINDOW_H
#define GRACEFUL_WM_WINDOW_H
#include "types.h"


void window_free (GWMWindow* win);
void window_update_role(GWMWindow *win, xcb_get_property_reply_t *prop);
void window_update_icon(GWMWindow *win, xcb_get_property_reply_t *prop);
void window_update_name (GWMWindow* win, xcb_get_property_reply_t* prop);
void window_update_class (GWMWindow* win, xcb_get_property_reply_t* prop);
void window_update_leader (GWMWindow* win, xcb_get_property_reply_t* prop);
void window_update_machine(GWMWindow *win, xcb_get_property_reply_t *prop);
void window_update_type(GWMWindow *window, xcb_get_property_reply_t *reply);
void window_update_name_legacy(GWMWindow* win, xcb_get_property_reply_t* prop);
void window_update_strut_partial(GWMWindow *win, xcb_get_property_reply_t *prop);
void window_update_transient_for (GWMWindow* win, xcb_get_property_reply_t* prop);
void window_update_hints(GWMWindow *win, xcb_get_property_reply_t *prop, bool *urgency_hint);
bool window_update_normal_hints(GWMWindow *win, xcb_get_property_reply_t *reply, xcb_get_geometry_reply_t *geom);
bool window_update_motif_hints(GWMWindow *win, xcb_get_property_reply_t *prop, GWMBorderStyle *motif_border_style);

#endif //GRACEFUL_WM_WINDOW_H
