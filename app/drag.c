//
// Created by dingjing on 23-12-10.
//

#include "val.h"
#include "log.h"
#include "dpi.h"
#include "drag.h"
#include "utils.h"
#include "cursor.h"
#include "handlers.h"
#include "container.h"


struct drag_x11_cb
{
    ev_prepare                      prepare;
    GWMDragResult                   result;             // Whether this modal event loop should be exited and with which result.
    GWMContainer*                   con;                // The container that is being dragged or resized, or NULL if this is a
                                                        // drag of the resize handle. */
    const xcb_button_press_event_t* event;              // The original event that initiated the drag.
    GWMRect                         oldRect;            // The dimensions of con when the loop was started.
    CallbackCB                      callback;           // The callback to invoke after every pointer movement.
    bool                            thresholdExceeded;  // Drag distance threshold exceeded.
                                                        // If use_threshold is not set, then threshold_exceeded is always true.
    xcb_cursor_t                    xCursor;            // Cursor to set after the threshold is exceeded.
    const void*                     extra;              // User data pointer for callback.
};


static bool drain_drag_events(EV_P, struct drag_x11_cb *dragLoop);
static void xcb_drag_prepare_cb(EV_P_ ev_prepare *w, int rEvents);
    static bool threshold_exceeded(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);


GWMDragResult drag_pointer(GWMContainer* con, const xcb_button_press_event_t *event, xcb_window_t confineTo, int cursor, bool useThreshold, CallbackCB callback, const void *extra)
{
    xcb_cursor_t xcursor = cursor ? cursor_get_cursor(cursor) : XCB_NONE;

    xcb_generic_error_t*        error;
    xcb_grab_pointer_reply_t*   reply;
    xcb_grab_pointer_cookie_t   cookie;

    cookie = xcb_grab_pointer(gConn, false, gRoot, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, confineTo, useThreshold ? XCB_NONE : xcursor, XCB_CURRENT_TIME);
    if ((reply = xcb_grab_pointer_reply(gConn, cookie, &error)) == NULL) {
        ERROR("Could not grab pointer (error_code = %d)", error->error_code);
        free(error);
        return DRAG_ABORT;
    }

    free(reply);

    xcb_grab_keyboard_reply_t*  keyBReply;
    xcb_grab_keyboard_cookie_t  keyBCookie;

    keyBCookie = xcb_grab_keyboard(gConn, false, gRoot, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    if ((keyBReply = xcb_grab_keyboard_reply(gConn, keyBCookie, &error)) == NULL) {
        ERROR("Could not grab keyboard (error_code = %d)", error->error_code);
        free(error);
        xcb_ungrab_pointer(gConn, XCB_CURRENT_TIME);
        return DRAG_ABORT;
    }
    free(keyBReply);

    /* Go into our own event loop */
    struct drag_x11_cb loop = {
        .result = DRAGGING,
        .con = con,
        .event = event,
        .callback = callback,
        .thresholdExceeded = !useThreshold,
        .xCursor = xcursor,
        .extra = extra,
    };
    ev_prepare *prepare = &loop.prepare;
    if (con) {
        loop.oldRect = con->rect;
    }

    ev_prepare_init(prepare, xcb_drag_prepare_cb);
    prepare->data = &loop;
    util_main_set_x11_cb(false);
    ev_prepare_start(gMainLoop, prepare);

    ev_loop(gMainLoop, 0);

    ev_prepare_stop(gMainLoop, prepare);
    util_main_set_x11_cb(true);

    xcb_ungrab_keyboard(gConn, XCB_CURRENT_TIME);
    xcb_ungrab_pointer(gConn, XCB_CURRENT_TIME);
    xcb_flush(gConn);

    return loop.result;
}

static bool threshold_exceeded(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
    /* The threshold is about the height of one window decoration. */
    const uint32_t threshold = dpi_logical_px(15);
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2) > threshold * threshold;
}

static bool drain_drag_events(EV_P, struct drag_x11_cb *dragLoop)
{
    xcb_motion_notify_event_t *last_motion_notify = NULL;
    xcb_generic_event_t *event;

    while ((event = xcb_poll_for_event(gConn)) != NULL) {
        if (event->response_type == 0) {
            xcb_generic_error_t *error = (xcb_generic_error_t *) event;
            DEBUG("X11 Error received (probably harmless)! sequence 0x%x, error_code = %d", error->sequence, error->error_code);
            free(event);
            continue;
        }

        int type = (event->response_type & 0x7F);
        switch (type) {
            case XCB_BUTTON_RELEASE: {
                dragLoop->result = DRAG_SUCCESS;
                break;
            }
            case XCB_KEY_PRESS: {
                DEBUG("A key was pressed during drag, reverting changes.");
                dragLoop->result = DRAG_REVERT;
                handler_handle_event(type, event);
                break;
            }
            case XCB_UNMAP_NOTIFY: {
                xcb_unmap_notify_event_t *unmap_event = (xcb_unmap_notify_event_t *)event;
                GWMContainer* con = container_by_window_id(unmap_event->window);
                if (con != NULL) {
                    DEBUG("UnmapNotify for window 0x%08x (container %p)", unmap_event->window, con);
                    if (container_get_workspace(con) == container_get_workspace(gFocused)) {
                        DEBUG("UnmapNotify for a managed window on the current workspace, aborting");
                        dragLoop->result = DRAG_ABORT;
                    }
                }
                handler_handle_event(type, event);
                break;
            }
            case XCB_MOTION_NOTIFY: {
                FREE(last_motion_notify);
                last_motion_notify = (xcb_motion_notify_event_t *)event;
                break;
            }
            default: {
                DEBUG("Passing to original handler");
                handler_handle_event(type, event);
                break;
            }
        }

        if (last_motion_notify != (xcb_motion_notify_event_t *)event)
            free(event);

        if (dragLoop->result != DRAGGING) {
            ev_break(EV_A_ EVBREAK_ONE);
            if (dragLoop->result == DRAG_SUCCESS) {
                break;
            }
            else {
                free(last_motion_notify);
                return true;
            }
        }
    }

    if (last_motion_notify == NULL) {
        return true;
    }

    if (!dragLoop->thresholdExceeded && threshold_exceeded(last_motion_notify->root_x, last_motion_notify->root_y, dragLoop->event->root_x, dragLoop->event->root_y)) {
        if (dragLoop->xCursor != XCB_NONE) {
            xcb_change_active_pointer_grab(gConn, dragLoop->xCursor, XCB_CURRENT_TIME, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION);
        }
        dragLoop->thresholdExceeded = true;
    }

    if (dragLoop->thresholdExceeded && (!dragLoop->con || container_exists(dragLoop->con))) {
        dragLoop->callback(dragLoop->con, &(dragLoop->oldRect), last_motion_notify->root_x, last_motion_notify->root_y, dragLoop->event, dragLoop->extra);
    }
    FREE(last_motion_notify);

    xcb_flush(gConn);

    return dragLoop->result != DRAGGING;
}

static void xcb_drag_prepare_cb(EV_P_ ev_prepare *w, int rEvents)
{
    struct drag_x11_cb *dragLoop = (struct drag_x11_cb *)w->data;
    while (!drain_drag_events(EV_A, dragLoop)) {

    }
}