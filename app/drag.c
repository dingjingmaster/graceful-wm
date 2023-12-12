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
#include "workspace.h"
#include "output.h"
#include "x.h"
#include "tree.h"
#include "randr.h"
#include "xcb.h"
#include "render.h"
#include "move.h"

typedef enum
{
    DT_SIBLING,
    DT_CENTER,
    DT_PARENT
} drop_type_t;

struct callback_params
{
    xcb_window_t*   indicator;
    GWMContainer**  target;
    GWMDirection*   direction;
    drop_type_t*    dropType;
};

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


static bool is_tiling_drop_target(GWMContainer* con);
static xcb_window_t create_drop_indicator(GWMRect rect);
static GWMContainer* find_drop_target(uint32_t x, uint32_t y);
static bool drain_drag_events(EV_P, struct drag_x11_cb *dragLoop);
static void xcb_drag_prepare_cb(EV_P_ ev_prepare *w, int rEvents);
static bool con_on_side_of_parent(GWMContainer* con, GWMDirection direction);
static bool threshold_exceeded(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);
static GWMRect adjust_rect(GWMRect rect, GWMDirection direction, uint32_t threshold);


GWMDragResult drag_pointer(GWMContainer* con, const xcb_button_press_event_t *event, xcb_window_t confineTo, int cursor, bool useThreshold, CallbackCB callback, const void *extra)
{
    xcb_cursor_t xcursor = cursor ? cursor_get_cursor(cursor) : XCB_NONE;

    xcb_generic_error_t*        error;
    xcb_grab_pointer_reply_t*   reply;
    xcb_grab_pointer_cookie_t   cookie;

    cookie = xcb_grab_pointer(gConn, false, gRoot, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, confineTo, useThreshold ? XCB_NONE : xcursor, XCB_CURRENT_TIME);
    if ((reply = xcb_grab_pointer_reply(gConn, cookie, &error)) == NULL) {
        ERROR("Could not grab pointer (error_code = %d)", error->error_code);
        FREE(error);
        return DRAG_ABORT;
    }

    FREE(reply);

    xcb_grab_keyboard_reply_t*  keyBReply;
    xcb_grab_keyboard_cookie_t  keyBCookie;

    keyBCookie = xcb_grab_keyboard(gConn, false, gRoot, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    if ((keyBReply = xcb_grab_keyboard_reply(gConn, keyBCookie, &error)) == NULL) {
        ERROR("Could not grab keyboard (error_code = %d)", error->error_code);
        FREE(error);
        xcb_ungrab_pointer(gConn, XCB_CURRENT_TIME);
        return DRAG_ABORT;
    }
    FREE(keyBReply);

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
    main_set_x11_cb(false);
    ev_prepare_start(gMainLoop, prepare);

    ev_loop(gMainLoop, 0);

    ev_prepare_stop(gMainLoop, prepare);
    main_set_x11_cb(true);

    xcb_ungrab_keyboard(gConn, XCB_CURRENT_TIME);
    xcb_ungrab_pointer(gConn, XCB_CURRENT_TIME);
    xcb_flush(gConn);

    return loop.result;
}

bool tiling_drag_has_drop_targets(void)
{
    int drop_targets = 0;
    GWMContainer* con;
    TAILQ_FOREACH (con, &gAllContainer, allContainers) {
        if (!is_tiling_drop_target(con)) {
            continue;
        }
        drop_targets++;
    }

    GWMContainer* output;
    TAILQ_FOREACH (output, &(gContainerRoot->focusHead), focused) {
        if (container_is_internal(output)) {
            continue;
        }
        GWMContainer* visible_ws = NULL;
        GREP_FIRST(visible_ws, output_get_content(output), workspace_is_visible(child));
        if (visible_ws != NULL && container_num_children(visible_ws) == 0) {
            drop_targets++;
        }
    }

    return drop_targets > 1;
}

DRAGGING_CB(drag_callback) {
    /* 30% of the container (minus the parent indicator) is used to drop the
     * dragged container as a sibling to the target */
    const double sibling_indicator_percent_of_rect = 0.3;
    /* Use the base decoration height and add a few pixels. This makes the
     * outer indicator generally thin but at least thick enough to cover
     * container titles */
    const double parent_indicator_max_size = render_deco_height() + dpi_logical_px(5);

    GWMContainer *target = find_drop_target(new_x, new_y);
    if (target == NULL) {
        return;
    }

    GWMRect rect = target->rect;

    GWMDirection direction = 0;
    drop_type_t drop_type = DT_CENTER;
    bool draw_window = true;
    const struct callback_params *params = extra;

    if (target->type == CT_WORKSPACE) {
        goto create_indicator;
    }

    /* Define the thresholds in pixels. The drop type depends on the cursor
     * position. */
    const uint32_t min_rect_dimension = MIN(rect.width, rect.height);
    const uint32_t sibling_indicator_size = MAX(dpi_logical_px(2), (uint32_t)(sibling_indicator_percent_of_rect * min_rect_dimension));
    const uint32_t parent_indicator_size = MIN(
        parent_indicator_max_size,
        /* For small containers, start where the sibling indicator finishes.
         * This is always at least 1 pixel. We use min() to not override the
         * sibling indicator: */
        sibling_indicator_size - 1);

    /* Find which edge the cursor is closer to. */
    const uint32_t d_left = new_x - rect.x;
    const uint32_t d_top = new_y - rect.y;
    const uint32_t d_right = rect.x + rect.width - new_x;
    const uint32_t d_bottom = rect.y + rect.height - new_y;
    const uint32_t d_min = MIN(MIN(d_left, d_right), MIN(d_top, d_bottom));
    /* And move the container towards that direction. */
    if (d_left == d_min) {
        direction = D_LEFT;
    } else if (d_top == d_min) {
        direction = D_UP;
    } else if (d_right == d_min) {
        direction = D_RIGHT;
    } else if (d_bottom == d_min) {
        direction = D_DOWN;
    } else {
        /* Keep the compiler happy */
        ERROR("min() is broken");
        g_assert(false);
    }
    const bool target_parent = (d_min < parent_indicator_size && con_on_side_of_parent(target, direction));
    const bool target_sibling = (d_min < sibling_indicator_size);
    drop_type = target_parent ? DT_PARENT : (target_sibling ? DT_SIBLING : DT_CENTER);

    /* target == con makes sense only when we are moving away from target's parent. */
    if (drop_type != DT_PARENT && target == con) {
        draw_window = false;
        xcb_destroy_window(gConn, *(params->indicator));
        *(params->indicator) = 0;
        goto create_indicator;
    }

    switch (drop_type) {
        case DT_PARENT:
            while (target->parent->type != CT_WORKSPACE && con_on_side_of_parent(target->parent, direction)) {
                target = target->parent;
            }
            rect = adjust_rect(target->parent->rect, direction, parent_indicator_size);
            break;
        case DT_CENTER:
            rect = target->rect;
            rect.x += sibling_indicator_size;
            rect.y += sibling_indicator_size;
            rect.width -= sibling_indicator_size * 2;
            rect.height -= sibling_indicator_size * 2;
            break;
        case DT_SIBLING:
            rect = adjust_rect(target->rect, direction, sibling_indicator_size);
            break;
    }

create_indicator:
    if (draw_window) {
        if (*(params->indicator) == 0) {
            *(params->indicator) = create_drop_indicator(rect);
        }
        else {
            const uint32_t values[4] = {rect.x, rect.y, rect.width, rect.height};
            const uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
            xcb_configure_window(gConn, *(params->indicator), mask, values);
        }
    }
    x_mask_event_mask(~XCB_EVENT_MASK_ENTER_WINDOW);
    xcb_flush(gConn);

    *(params->target) = target;
    *(params->direction) = direction;
    *(params->dropType) = drop_type;
}

void tiling_drag(GWMContainer *con, xcb_button_press_event_t *event, bool useThreshold)
{
    DEBUG("Start dragging tiled container: con = %p", con);
    bool set_focus = (con == gFocused);
    bool set_fs = con->fullScreenMode != CF_NONE;

    /* Don't change focus while dragging. */
    x_mask_event_mask(~XCB_EVENT_MASK_ENTER_WINDOW);
    xcb_flush(gConn);

    /* Indicate drop location while dragging. This blocks until the drag is completed. */
    GWMContainer* target = NULL;
    GWMDirection direction;
    drop_type_t drop_type;
    xcb_window_t indicator = 0;
    const struct callback_params params = {&indicator, &target, &direction, &drop_type};

    GWMDragResult drag_result = drag_pointer(con, event, XCB_NONE, CURSOR_MOVE, useThreshold, drag_callback, &params);

    /* Dragging is done. We don't need the indicator window any more. */
    xcb_destroy_window(gConn, indicator);

    if (drag_result == DRAG_REVERT
        || target == NULL
        || (target == con && drop_type != DT_PARENT)
        || !container_exists(target)) {
        DEBUG("drop aborted");
        return;
    }

    const GWMOrientation orientation = util_orientation_from_direction(direction);
    const GWMPosition position = util_position_from_direction(direction);
    const GWMLayout layout = orientation == VERT ? L_SPLIT_V : L_SPLIT_H;
    container_disable_full_screen(con);
    switch (drop_type) {
        case DT_CENTER:
            /* Also handles workspaces.*/
            DEBUG("drop to center of %p", target);
            container_move_to_target(con, target);
            break;
        case DT_SIBLING:
            DEBUG("drop %s %p", util_position_to_string(position), target);
            if (container_orientation(target->parent) != orientation) {
                /* If con and target are the only children of the same parent, we can just change
                 * the parent's layout manually and then move con to the correct position.
                 * tree_split checks for a parent with only one child so it would create a new
                 * parent with the new layout. */
                if (con->parent == target->parent && container_num_children(target->parent) == 2) {
                    target->parent->layout = layout;
                }
                else {
                    tree_split(target, orientation);
                }
            }

            move_insert_con_into(con, target, position);

//            ipc_send_window_event("move", con);
            break;
        case DT_PARENT: {
            const bool parent_tabbed_or_stacked = (target->parent->layout == L_TABBED || target->parent->layout == L_STACKED);
            DEBUG("drop %s (%s) of %s%p",
                    util_direction_to_string(direction),
                    util_position_to_string(position),
                    parent_tabbed_or_stacked ? "tabbed/stacked " : "",
                    target);
            if (parent_tabbed_or_stacked) {
                /* When dealing with tabbed/stacked the target can be in the
                 * middle of the container. Thus, after a directional move, con
                 * will still be bound to the tabbed/stacked parent. */
                if (position == BEFORE) {
                    target = TAILQ_FIRST(&(target->parent->nodesHead));
                }
                else {
                    target = TAILQ_LAST(&(target->parent->nodesHead), nodesHead);
                }
            }
            if (con != target) {
                move_insert_con_into(con, target, position);
            }
            /* tree_move can change the focus */
            GWMContainer* old_focus = gFocused;
            move_tree_move(con, direction);
            if (gFocused != old_focus) {
                container_activate(old_focus);
            }
            break;
        }
    }
    /* Warning: target might not exist anymore */
    target = NULL;

    /* Manage fullscreen status. */
    if (set_focus || set_fs) {
        GWMContainer* fs = container_get_full_screen_covering_ws(container_get_workspace(con));
        if (fs == con) {
            ERROR("dragged container somehow got fullscreen again.");
            g_assert(false);
        }
        else if (fs && set_focus && set_fs) {
            /* con was focused & fullscreen, disable other fullscreen container. */
            container_disable_full_screen(fs);
        }
        else if (fs) {
            /* con was not focused, prefer other fullscreen container. */
            set_fs = set_focus = false;
        }
        else if (!set_focus) {
            /* con was not focused. If it was fullscreen and we are moving it to the focused
             * workspace we must focus it. */
            set_focus = (set_fs && container_get_workspace(gFocused) == container_get_workspace(con));
        }
    }
    if (set_fs) {
        container_enable_full_screen(con, CF_OUTPUT);
    }
    if (set_focus) {
        workspace_show(container_get_workspace(con));
        container_focus(con);
    }
    tree_render();
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
            FREE(event);
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
            FREE(event);

        if (dragLoop->result != DRAGGING) {
            ev_break(EV_A_ EVBREAK_ONE);
            if (dragLoop->result == DRAG_SUCCESS) {
                break;
            }
            else {
                FREE(last_motion_notify);
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

static bool is_tiling_drop_target(GWMContainer* con)
{
    if (!container_has_managed_window(con) || container_is_floating(con) || container_is_hidden(con)) {
        return false;
    }

    GWMContainer* ws = container_get_workspace(con);
    if (container_is_internal(ws)) {
        return false;
    }

    if (!workspace_is_visible(ws)) {
        return false;
    }

    GWMContainer* fs = container_get_full_screen_covering_ws(ws);
    if (fs != NULL && fs != con) {
        return false;
    }

    return true;
}

static bool con_on_side_of_parent(GWMContainer* con, GWMDirection direction)
{
    const GWMOrientation orientation = util_orientation_from_direction(direction);
    GWMDirection reverse_direction;
    switch (direction) {
        case D_LEFT:
            reverse_direction = D_RIGHT;
            break;
        case D_RIGHT:
            reverse_direction = D_LEFT;
            break;
        case D_UP:
            reverse_direction = D_DOWN;
            break;
        case D_DOWN:
            reverse_direction = D_UP;
            break;
    }

    return (container_orientation(con->parent) != orientation || con->parent->layout == L_STACKED || con->parent->layout == L_TABBED || container_descend_direction(con->parent, reverse_direction) == con);
}

static GWMContainer* find_drop_target(uint32_t x, uint32_t y)
{
    GWMContainer* con;
    TAILQ_FOREACH (con, &gAllContainer, allContainers) {
        GWMRect rect = con->rect;
        if (!util_rect_contains(rect, x, y) || !is_tiling_drop_target(con)) {
            continue;
        }
        GWMContainer* ws = container_get_workspace(con);
        GWMContainer* fs = container_get_full_screen_covering_ws(ws);
        return fs ? fs : con;
    }

    /* Couldn't find leaf container, get a workspace. */
    GWMOutput *output = randr_get_output_containing(x, y);
    if (!output) {
        return NULL;
    }
    GWMContainer* content = output_get_content(output->container);

    /* Still descend because you can drag to the bar on an non-empty workspace. */
    return container_descend_tiling_focused(content);
}

static xcb_window_t create_drop_indicator(GWMRect rect)
{
    uint32_t mask = 0;
    uint32_t values[2];

    mask = XCB_CW_BACK_PIXEL;
    values[0] = gConfig.client.focused.indicator.colorPixel;

    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[1] = 1;

    xcb_window_t indicator = xcb_gwm_create_window(gConn, rect, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_WINDOW_CLASS_INPUT_OUTPUT, CURSOR_MOVE, false, mask, values);
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, indicator, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, (strlen("gwm-drag") + 1) * 2, "gwm-drag\0gwm-drag\0");
    xcb_map_window(gConn, indicator);
    xcb_circulate_window(gConn, XCB_CIRCULATE_RAISE_LOWEST, indicator);

    return indicator;
}

static GWMRect adjust_rect(GWMRect rect, GWMDirection direction, uint32_t threshold)
{
    switch (direction) {
        case D_LEFT: {
            rect.width = threshold;
            break;
        }
        case D_UP: {
            rect.height = threshold;
            break;
        }
        case D_RIGHT: {
            rect.x += (rect.width - threshold);
            rect.width = threshold;
            break;
        }
        case D_DOWN: {
            rect.y += (rect.height - threshold);
            rect.height = threshold;
            break;
        }
    }
    return rect;
}