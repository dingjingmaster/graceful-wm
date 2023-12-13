//
// Created by dingjing on 23-12-10.
//

#include "resize.h"

#include "val.h"
#include "log.h"
#include "drag.h"
#include "container.h"
#include "x.h"
#include "xcb.h"
#include "dpi.h"
#include "tree.h"
#include "utils.h"


struct CallbackParams
{
    GWMOrientation  orientation;
    GWMContainer*   output;
    xcb_window_t    helpwin;
    uint32_t*       newPosition;
    bool*           thresholdExceeded;
};

DRAGGING_CB(resizeCallback)
{
    const struct CallbackParams* params = extra;
    GWMContainer* output = params->output;
    DEBUG("new x = %d, y = %d", new_x, new_y);

    if (!*params->thresholdExceeded) {
        xcb_map_window(gConn, params->helpwin);
        if (params->orientation == HORIZON) {
            xcb_warp_pointer(gConn, XCB_NONE, event->root, 0, 0, 0, 0, *params->newPosition + new_x - event->root_x, new_y);
        }
        else {
            xcb_warp_pointer(gConn, XCB_NONE, event->root, 0, 0, 0, 0, new_x, *params->newPosition + new_y - event->root_y);
        }
        *params->thresholdExceeded = true;
        return;
    }

    if (params->orientation == HORIZON) {
        if (new_x > (output->rect.x + output->rect.width - 25) || new_x < (output->rect.x + 25)) {
            return;
        }
        *(params->newPosition) = new_x;
        xcb_configure_window(gConn, params->helpwin, XCB_CONFIG_WINDOW_X, params->newPosition);
    }
    else {
        if (new_y > (output->rect.y + output->rect.height - 25) || new_y < (output->rect.y + 25)) {
            return;
        }

        *(params->newPosition) = new_y;
        xcb_configure_window(gConn, params->helpwin, XCB_CONFIG_WINDOW_Y, params->newPosition);
    }

    xcb_flush(gConn);
}


double resize_percent_for_1px(GWMContainer *con)
{
    const int parent_size = container_rect_size_in_orientation(con->parent);
    const int min_size = (container_orientation(con->parent) == HORIZON ? 1 : 1 + con->decorationRect.height);

    return ((double)min_size / (double)parent_size);
}

bool resize_neighboring_cons(GWMContainer *first, GWMContainer *second, int px, int ppt)
{
    g_assert(px * ppt == 0);

    GWMContainer* parent = first->parent;
    double new_first_percent;
    double new_second_percent;
    if (ppt) {
        new_first_percent = first->percent + ((double)ppt / 100.0);
        new_second_percent = second->percent - ((double)ppt / 100.0);
    }
    else {
        const double pct = (double)px / (double)container_rect_size_in_orientation(first->parent);
        new_first_percent = first->percent + pct;
        new_second_percent = second->percent - pct;
    }
    /* Ensure that no container will be less than 1 pixel in the resizing
     * direction. */
    if (new_first_percent < resize_percent_for_1px(first) || new_second_percent < resize_percent_for_1px(second)) {
        return false;
    }

    first->percent = new_first_percent;
    second->percent = new_second_percent;
    container_fix_percent(parent);

    return true;
}

bool resize_find_tiling_participants(GWMContainer **current, GWMContainer **other, GWMDirection direction, bool bothSides)
{
    DEBUG("Find two participants for resizing container=%p in direction=%i", other, direction);
    GWMContainer* first = *current;
    GWMContainer* second = NULL;
    if (first == NULL) {
        DEBUG("Current container is NULL, aborting.");
        return false;
    }

    const GWMOrientation  search_orientation = util_orientation_from_direction(direction);
    const bool dir_backwards = (direction == D_UP || direction == D_LEFT);
    while (first->type != CT_WORKSPACE && first->type != CT_FLOATING_CON && second == NULL) {
        if ((container_orientation(first->parent) != search_orientation)
            || (first->parent->layout == L_STACKED) || (first->parent->layout == L_TABBED)) {
            first = first->parent;
            continue;
        }

        /* get the counterpart for this resizement */
        if (dir_backwards) {
            second = TAILQ_PREV(first, nodesHead, nodes);
            if (second == NULL && bothSides == true) {
                second = TAILQ_NEXT(first, nodes);
            }
        }
        else {
            second = TAILQ_NEXT(first, nodes);
            if (second == NULL && bothSides == true) {
                second = TAILQ_PREV(first, nodesHead, nodes);
            }
        }

        if (second == NULL) {
            DEBUG("No second container in this direction found, trying to look further up in the tree...");
            first = first->parent;
        }
    }

    DEBUG("Found participants: first=%p and second=%p.", first, second);
    *current = first;
    *other = second;
    if (first == NULL || second == NULL) {
        DEBUG("Could not find two participants for this resize request.");
        return false;
    }

    return true;
}

void resize_graphical_handler(GWMContainer *first, GWMContainer *second, GWMOrientation orientation, const xcb_button_press_event_t *event, bool useThreshold)
{
    GWMContainer *output = container_get_output(first);
    DEBUG("x = %d, width = %d", output->rect.x, output->rect.width);
    DEBUG("first = %p / %s", first, first->name);
    DEBUG("second = %p / %s", second, second->name);

    x_mask_event_mask(~XCB_EVENT_MASK_ENTER_WINDOW);
    xcb_flush(gConn);

    uint32_t mask = 0;
    uint32_t values[2];

    mask = XCB_CW_OVERRIDE_REDIRECT;
    values[0] = 1;

    xcb_window_t grabwin = xcb_gwm_create_window(gConn, output->rect, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_WINDOW_CLASS_INPUT_ONLY, CURSOR_POINTER, true, mask, values);

    uint32_t initial_position, new_position;

    GWMRect helprect;
    helprect.x = second->rect.x;
    helprect.y = second->rect.y;
    GWMContainer* ffirst = container_descend_focused(first);
    GWMContainer* fsecond = container_descend_focused(second);
    if (orientation == HORIZON) {
        helprect.width = dpi_logical_px(2);
        helprect.height = second->rect.height;
        const uint32_t ffirst_right = ffirst->rect.x + ffirst->rect.width;
        const uint32_t gap = (fsecond->rect.x - ffirst_right);
        const uint32_t middle = fsecond->rect.x - (gap / 2);
        DEBUG("ffirst->rect = {.x = %u, .width = %u}", ffirst->rect.x, ffirst->rect.width);
        DEBUG("fsecond->rect = {.x = %u, .width = %u}", fsecond->rect.x, fsecond->rect.width);
        DEBUG("gap = %u, middle = %u", gap, middle);
        initial_position = middle;
    }
    else {
        helprect.width = second->rect.width;
        helprect.height = dpi_logical_px(2);
        const uint32_t ffirst_bottom = ffirst->rect.y + ffirst->rect.height;
        const uint32_t gap = (fsecond->rect.y - ffirst_bottom);
        const uint32_t middle = fsecond->rect.y - (gap / 2);
        DEBUG("ffirst->rect = {.y = %u, .height = %u}", ffirst->rect.y, ffirst->rect.height);
        DEBUG("fsecond->rect = {.y = %u, .height = %u}", fsecond->rect.y, fsecond->rect.height);
        DEBUG("gap = %u, middle = %u", gap, middle);
        initial_position = middle;
    }

    mask = XCB_CW_BACK_PIXEL;
    values[0] = gConfig.client.focused.border.colorPixel;

    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[1] = 1;

    xcb_window_t helpwin = xcb_gwm_create_window(gConn, helprect, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_WINDOW_CLASS_INPUT_OUTPUT, (orientation == HORIZON ? CURSOR_RESIZE_HORIZONTAL : CURSOR_RESIZE_VERTICAL), false, mask, values);

    if (!useThreshold) {
        xcb_map_window(gConn, helpwin);
        if (orientation == HORIZON) {
            xcb_warp_pointer(gConn, XCB_NONE, event->root, 0, 0, 0, 0, initial_position, event->root_y);
        }
        else {
            xcb_warp_pointer(gConn, XCB_NONE, event->root, 0, 0, 0, 0, event->root_x, initial_position);
        }
    }

    xcb_circulate_window(gConn, XCB_CIRCULATE_RAISE_LOWEST, helpwin);

    xcb_flush(gConn);

    new_position = initial_position;

    bool threshold_exceeded = !useThreshold;

    const struct CallbackParams params = {orientation, output, helpwin, &new_position, &threshold_exceeded};

    tree_render();

    GWMDragResult drag_result = drag_pointer(NULL, event, grabwin, 0, useThreshold, resizeCallback, &params);

    xcb_destroy_window(gConn, helpwin);
    xcb_destroy_window(gConn, grabwin);
    xcb_flush(gConn);

    if (drag_result == DRAG_REVERT) {
        return;
    }

    int pixels = (new_position - initial_position);
    DEBUG("Done, pixels = %d", pixels);

    if (pixels == 0) {
        return;
    }

    g_assert(first->percent > 0.0);
    g_assert(second->percent > 0.0);
    const bool result = resize_neighboring_cons(first, second, pixels, 0);

    DEBUG("Graphical resize %s: first->percent = %f, second->percent = %f.", result ? "successful" : "failed", first->percent, second->percent);
}
