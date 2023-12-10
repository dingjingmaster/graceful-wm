//
// Created by dingjing on 23-11-27.
//

#include "xcb.h"

#include "log.h"
#include "val.h"
#include "types.h"
#include "cursor.h"
#include "handlers.h"


xcb_visualid_t xcb_gwm_get_visualid_by_depth(uint16_t depth)
{
    xcb_depth_iterator_t depth_iter;

    depth_iter = xcb_screen_allowed_depths_iterator(gRootScreen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        if (depth_iter.data->depth != depth) {
            continue;
        }

        xcb_visualtype_iterator_t visual_iter;
        visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
        if (!visual_iter.rem) {
            continue;
        }
        return visual_iter.data->visual_id;
    }
    return 0;
}

void xcb_gwm_fake_configure_notify(xcb_connection_t *conn, xcb_rectangle_t r, xcb_window_t window, int borderWidth)
{
    void* event = calloc(32, 1);
    xcb_configure_notify_event_t *generated_event = event;

    generated_event->event = window;
    generated_event->window = window;
    generated_event->response_type = XCB_CONFIGURE_NOTIFY;

    generated_event->x = r.x;
    generated_event->y = r.y;
    generated_event->width = r.width;
    generated_event->height = r.height;

    generated_event->border_width = borderWidth;
    generated_event->above_sibling = XCB_NONE;
    generated_event->override_redirect = false;

    xcb_send_event(conn, false, window, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char *)generated_event);
    xcb_flush(conn);

    free(event);
}

void xcb_gwm_fake_absolute_configure_notify(GWMContainer *con)
{
    xcb_rectangle_t absolute;
    if (con->window == NULL)
        return;

    absolute.x = con->rect.x + con->windowRect.x;
    absolute.y = con->rect.y + con->windowRect.y;
    absolute.width = con->windowRect.width;
    absolute.height = con->windowRect.height;

    DEBUG("fake rect = (%d, %d, %d, %d)\n", absolute.x, absolute.y, absolute.width, absolute.height);

    xcb_gwm_fake_configure_notify(gConn, absolute, con->window->id, con->borderWidth);
}

xcb_visibility_t *xcb_gwm_get_visual_type_by_id(xcb_visualid_t visualID)
{
    xcb_depth_iterator_t depth_iter;

    depth_iter = xcb_screen_allowed_depths_iterator(gRootScreen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        xcb_visualtype_iterator_t visual_iter;
        visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
        for (; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
            if (visualID == visual_iter.data->visual_id) {
                return visual_iter.data;
            }
        }
    }

    return 0;
}

void xcb_gwm_send_take_focus(xcb_window_t window, xcb_timestamp_t timeStamp)
{
    void *event = calloc(32, 1);
    xcb_client_message_event_t *ev = event;

    ev->response_type = XCB_CLIENT_MESSAGE;
    ev->window = window;
    ev->type = A_WM_PROTOCOLS;
    ev->format = 32;
    ev->data.data32[0] = A_WM_TAKE_FOCUS;
    ev->data.data32[1] = timeStamp;

    DEBUG("Sending WM_TAKE_FOCUS to the client");
    xcb_send_event(gConn, false, window, XCB_EVENT_MASK_NO_EVENT, (char *)ev);

    free(event);
}

xcb_atom_t xcb_gwm_get_preferred_window_type(xcb_get_property_reply_t *reply)
{
    if (reply == NULL || xcb_get_property_value_length(reply) == 0) {
        return XCB_NONE;
    }

    xcb_atom_t *atoms;
    if ((atoms = xcb_get_property_value(reply)) == NULL) {
        return XCB_NONE;
    }

    for (int i = 0; i < xcb_get_property_value_length(reply) / (reply->format / 8); i++) {
        if (atoms[i] == A__NET_WM_WINDOW_TYPE_NORMAL
            || atoms[i] == A__NET_WM_WINDOW_TYPE_DIALOG
            || atoms[i] == A__NET_WM_WINDOW_TYPE_UTILITY
            || atoms[i] == A__NET_WM_WINDOW_TYPE_TOOLBAR
            || atoms[i] == A__NET_WM_WINDOW_TYPE_SPLASH
            || atoms[i] == A__NET_WM_WINDOW_TYPE_MENU
            || atoms[i] == A__NET_WM_WINDOW_TYPE_DROPDOWN_MENU
            || atoms[i] == A__NET_WM_WINDOW_TYPE_POPUP_MENU
            || atoms[i] == A__NET_WM_WINDOW_TYPE_TOOLTIP
            || atoms[i] == A__NET_WM_WINDOW_TYPE_NOTIFICATION) {
            return atoms[i];
        }
    }

    return XCB_NONE;
}

bool xcb_gwm_reply_contains_atom(xcb_get_property_reply_t *prop, xcb_atom_t atom)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        return false;
    }

    xcb_atom_t *atoms;
    if ((atoms = xcb_get_property_value(prop)) == NULL) {
        return false;
    }

    for (int i = 0; i < xcb_get_property_value_length(prop) / (prop->format / 8); i++) {
        if (atoms[i] == atom) {
            return true;
        }
    }

    return false;
}

void xcb_gwm_grab_buttons(xcb_connection_t *conn, xcb_window_t window, int *buttons)
{
    int i = 0;
    while (buttons[i] > 0) {
        xcb_grab_button(conn, false, window, XCB_EVENT_MASK_BUTTON_PRESS, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, gRoot, XCB_NONE, buttons[i], XCB_BUTTON_MASK_ANY);
        i++;
    }
}

void xcb_gwm_set_window_rect(xcb_connection_t *conn, xcb_window_t window, GWMRect r)
{
    xcb_void_cookie_t cookie = xcb_configure_window(conn, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, &(r.x));
    handler_add_ignore_event(cookie.sequence, -1);
}

void xcb_gwm_add_property_atom(xcb_connection_t *conn, xcb_window_t window, xcb_atom_t property, xcb_atom_t atom)
{
    xcb_change_property(conn, XCB_PROP_MODE_APPEND, window, property, XCB_ATOM_ATOM, 32, 1, (uint32_t[]){atom});
}

void xcb_gwm_remove_property_atom(xcb_connection_t *conn, xcb_window_t window, xcb_atom_t property, xcb_atom_t atom)
{
    xcb_grab_server(conn);

    xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, xcb_get_property(conn, false, window, property, XCB_GET_PROPERTY_TYPE_ANY, 0, 4096), NULL);
    if (reply == NULL || xcb_get_property_value_length(reply) == 0) {
        goto release_grab;
    }
    xcb_atom_t *atoms = xcb_get_property_value(reply);
    if (atoms == NULL) {
        goto release_grab;
    }

    {
        int num = 0;
        const int current_size = xcb_get_property_value_length(reply) / (reply->format / 8);
        xcb_atom_t values[current_size];
        for (int i = 0; i < current_size; i++) {
            if (atoms[i] != atom) {
                values[num++] = atoms[i];
            }
        }

        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, property, XCB_ATOM_ATOM, 32, num, values);
    }

release_grab:
    FREE(reply);
    xcb_ungrab_server(conn);
}

xcb_window_t xcb_gwm_create_window(xcb_connection_t *conn, GWMRect dims, uint16_t depth, xcb_visualid_t visual, uint16_t windowClass, GWMCursor cursor, bool map, uint32_t mask, uint32_t *values)
{
    xcb_window_t result = xcb_generate_id(conn);

    if (windowClass == XCB_WINDOW_CLASS_INPUT_ONLY) {
        depth = XCB_COPY_FROM_PARENT;
        visual = XCB_COPY_FROM_PARENT;
    }

    xcb_void_cookie_t gc_cookie = xcb_create_window(conn, depth, result, gRoot, dims.x, dims.y, dims.width, dims.height, 0, windowClass, visual, mask, values);

    xcb_generic_error_t *error = xcb_request_check(conn, gc_cookie);
    if (error != NULL) {
        ERROR(_("Could not create window. Error code: %d."), error->error_code);
    }

    /* Set the cursor */
    uint32_t cursor_values[] = {cursor_get_cursor(cursor)};
    xcb_change_window_attributes(conn, result, XCB_CW_CURSOR, cursor_values);

    /* Map the window (= make it visible) */
    if (map) {
        xcb_map_window(conn, result);
    }

    return result;
}
