//
// Created by dingjing on 23-11-27.
//

#include "floating.h"

#include "x.h"
#include "val.h"
#include "log.h"
#include "drag.h"
#include "move.h"
#include "utils.h"
#include "randr.h"
#include "render.h"
#include "output.h"
#include "workspace.h"
#include "container.h"


struct resize_window_callback_params
{
    const GWMBorder corner;
    const bool proportional;
};


static GWMRect total_outputs_dimensions(void);
static void floating_set_hint_atom(GWMContainer* con, bool floating);


DRAGGING_CB(dragWindowCB)
{
    con->rect.x = oldRect->x + (new_x - event->root_x);
    con->rect.y = oldRect->y + (new_y - event->root_y);

    render_container(con);
    x_push_node(con);
    xcb_flush(gConn);

    if (!floating_maybe_reassign_ws(con)) {
        return;
    }
    x_set_warp_to(NULL);
    tree_render();
}

DRAGGING_CB(resizeWindowCB)
{
    const struct resize_window_callback_params *params = extra;
    GWMBorder corner = params->corner;

    int32_t dest_x = con->rect.x;
    int32_t dest_y = con->rect.y;
    uint32_t dest_width;
    uint32_t dest_height;

    double ratio = (double)oldRect->width / oldRect->height;

    if (corner & BORDER_LEFT) {
        dest_width = oldRect->width - (new_x - event->root_x);
    }
    else {
        dest_width = oldRect->width + (new_x - event->root_x);
    }

    if (corner & BORDER_TOP) {
        dest_height = oldRect->height - (new_y - event->root_y);
    }
    else {
        dest_height = oldRect->height + (new_y - event->root_y);
    }

    if (params->proportional) {
        dest_width = MAX(dest_width, (int)(dest_height * ratio));
        dest_height = MAX(dest_height, (int)(dest_width / ratio));
    }

    con->rect = (GWMRect){dest_x, dest_y, dest_width, dest_height};

    floating_check_size(con, false);

    if (corner & BORDER_LEFT) {
        dest_x = oldRect->x + (oldRect->width - con->rect.width);
    }

    if (corner & BORDER_TOP) {
        dest_y = oldRect->y + (oldRect->height - con->rect.height);
    }

    con->rect.x = dest_x;
    con->rect.y = dest_y;

    render_container(con);
    x_push_changes(gContainerRoot);
}

void floating_disable(GWMContainer *con)
{
    if (!container_is_floating(con)) {
        DEBUG("Container isn't floating, not doing anything.");
        return;
    }

    GWMContainer* ws = container_get_workspace(con);
    if (container_is_internal(ws)) {
        DEBUG("Can't disable floating for container in internal workspace.");
        return;
    }

    GWMContainer* tiling_focused = container_descend_tiling_focused(ws);
    if (tiling_focused->type == CT_WORKSPACE) {
        GWMContainer* parent = con->parent;
        container_detach(con);
        con->parent = NULL;
        tree_close_internal(parent, KILL_WINDOW_DO_NOT, true);
        container_attach(con, tiling_focused, false);
        con->percent = 0.0;
        container_fix_percent(con->parent);
    }
    else {
        move_insert_con_into(con, tiling_focused, AFTER);
    }

    con->floating = FLOATING_USER_OFF;
    floating_set_hint_atom(con, false);
//    ipc_send_window_event("floating", con);
}

void floating_raise_con(GWMContainer *con)
{
    DEBUG("Raising floating con %p / %s", con, con->name);
    TAILQ_REMOVE(&(con->parent->floatingHead), con, floatingWindows);
    TAILQ_INSERT_TAIL(&(con->parent->floatingHead), con, floatingWindows);
}

void floating_move_to_pointer(GWMContainer *con)
{
    g_assert(con->type == CT_FLOATING_CON);

    xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(gConn, xcb_query_pointer(gConn, gRoot), NULL);
    if (reply == NULL) {
        ERROR("could not query pointer position, not moving this container");
        return;
    }

    GWMOutput *output = randr_get_output_containing(reply->root_x, reply->root_y);
    if (output == NULL) {
        ERROR("The pointer is not on any output, cannot move the container here.");
        return;
    }

    int32_t x = reply->root_x - con->rect.width / 2;
    int32_t y = reply->root_y - con->rect.height / 2;
    FREE(reply);

    x = MAX(x, (int32_t)output->rect.x);
    y = MAX(y, (int32_t)output->rect.y);
    if (x + con->rect.width > output->rect.x + output->rect.width) {
        x = output->rect.x + output->rect.width - con->rect.width;
    }
    if (y + con->rect.height > output->rect.y + output->rect.height) {
        y = output->rect.y + output->rect.height - con->rect.height;
    }

    floating_reposition(con, (GWMRect){x, y, con->rect.width, con->rect.height});
}

bool floating_maybe_reassign_ws(GWMContainer *con)
{
    if (container_is_internal(container_get_workspace(con))) {
        DEBUG("Con in an internal workspace");
        return false;
    }

    GWMOutput *output = randr_get_output_from_rect(con->rect);
    if (!output) {
        ERROR("No output found at destination coordinates?");
        return false;
    }

    if (container_get_output(con) == output->container) {
        DEBUG("still the same ws");
        return false;
    }

    DEBUG("Need to re-assign!");

    GWMContainer* content = output_get_content(output->container);
    GWMContainer* ws = TAILQ_FIRST(&(content->focusHead));
    DEBUG("Moving con %p / %s to workspace %p / %s", con, con->name, ws, ws->name);
    GWMContainer* needs_focus = container_descend_focused(con);
    if (!container_inside_focused(needs_focus)) {
        needs_focus = NULL;
    }
    container_move_to_workspace(con, ws, false, true, false);
    if (needs_focus) {
        workspace_show(ws);
        container_activate(needs_focus);
    }
    return true;
}

void floating_center(GWMContainer *con, GWMRect rect)
{
    con->rect.x = rect.x + (rect.width / 2) - (con->rect.width / 2);
    con->rect.y = rect.y + (rect.height / 2) - (con->rect.height / 2);
}

bool floating_enable(GWMContainer *con, bool automatic)
{
    return false;
    bool set_focus = (con == gFocused);

    if (container_is_docked(con)) {
        DEBUG("Container is a dock window, not enabling floating mode.");
        return false;
    }

    if (container_is_floating(con)) {
        DEBUG("Container is already in floating mode, not doing anything.");
        return false;
    }

    if (con->type == CT_WORKSPACE) {
        DEBUG("Container is a workspace, not enabling floating mode.");
        return false;
    }

    GWMContainer* focus_head_placeholder = NULL;
    bool focus_before_parent = true;
    if (!set_focus) {
        GWMContainer* ancestor = con;
        while (ancestor->parent->type != CT_WORKSPACE) {
            focus_before_parent &= TAILQ_FIRST(&(ancestor->parent->focusHead)) == ancestor;
            ancestor = ancestor->parent;
        }

        if (focus_before_parent) {
            focus_head_placeholder = TAILQ_PREV(ancestor, focusHead, focused);
        }
        else {
            focus_head_placeholder = TAILQ_NEXT(ancestor, focused);
        }
    }

    container_detach(con);
    container_fix_percent(con->parent);

    GWMContainer *nc = container_new(NULL, NULL);
    GWMContainer *ws = container_get_workspace(con);
    nc->parent = ws;
    nc->type = CT_FLOATING_CON;
    nc->layout = L_SPLIT_H;
    TAILQ_INSERT_HEAD(&(ws->floatingHead), nc, floatingWindows);

    struct focusHead* fh = &(ws->focusHead);

    if (focus_before_parent) {
        if (focus_head_placeholder) {
            TAILQ_INSERT_AFTER(fh, focus_head_placeholder, nc, focused);
        }
        else {
            TAILQ_INSERT_HEAD(fh, nc, focused);
        }
    }
    else {
        if (focus_head_placeholder) {
            TAILQ_INSERT_BEFORE(focus_head_placeholder, nc, focused);
        }
        else {
            TAILQ_INSERT_TAIL(fh, nc, focused);
        }
    }

    if ((con->parent->type == CT_CON || con->parent->type == CT_FLOATING_CON) && container_num_children(con->parent) == 0) {
        DEBUG("Old container empty after setting this child to floating, closing");
        GWMContainer* parent = con->parent;
        con->parent = NULL;
        tree_close_internal(parent, KILL_WINDOW_DO_NOT, false);
    }

    g_autofree char *name = g_strdup_printf("[gwm con] floatingcon around %p", con);
    x_set_name(nc, name);

    DEBUG("Original rect: (%d, %d) with %d x %d", con->rect.x, con->rect.y, con->rect.width, con->rect.height);
    DEBUG("Geometry = (%d, %d) with %d x %d", con->geoRect.x, con->geoRect.y, con->geoRect.width, con->geoRect.height);
    nc->rect = con->geoRect;
    if (util_rect_equals(nc->rect, (GWMRect){0, 0, 0, 0})) {
        DEBUG("Geometry not set, combining children");
        GWMContainer* child;
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            DEBUG("child geometry: %d x %d", child->geoRect.width, child->geoRect.height);
            nc->rect.width += child->geoRect.width;
            nc->rect.height = MAX(nc->rect.height, child->geoRect.height);
        }
    }

    TAILQ_INSERT_TAIL(&(nc->nodesHead), con, nodes);
    TAILQ_INSERT_TAIL(&(nc->focusHead), con, focused);

    con->parent = nc;
    con->percent = 1.0;
    con->floating = FLOATING_USER_ON;

    if (automatic) {
        con->borderStyle = con->maxUserBorderStyle = gConfig.defaultFloatingBorder;
    }

    GWMRect bsr = container_border_style_rect(con);

    nc->rect.height -= bsr.height;
    nc->rect.width -= bsr.width;

    nc->rect.height += con->borderWidth * 2;
    nc->rect.width += con->borderWidth * 2;

    floating_check_size(nc, false);

    if (nc->rect.x == 0 && nc->rect.y == 0) {
        GWMContainer* leader;
        if (con->window && con->window->leader != XCB_NONE
            && con->window->id != con->window->leader
            && (leader = container_by_window_id(con->window->leader)) != NULL) {
            DEBUG("Centering above leader");
            floating_center(nc, leader->rect);
        }
        else {
            floating_center(nc, ws->rect);
        }
    }

    GWMOutput *current_output = randr_get_output_from_rect(nc->rect);
    GWMContainer* correct_output = container_get_output(ws);
    if (!current_output || current_output->container != correct_output) {
        DEBUG("This floating window is on the wrong output, fixing coordinates (currently (%d, %d))", nc->rect.x, nc->rect.y);
        if (current_output) {
            floating_fix_coordinates(nc, &current_output->container->rect, &correct_output->rect);
            current_output = randr_get_output_from_rect(nc->rect);
        }
        if (!current_output || current_output->container != correct_output) {
            floating_center(nc, ws->rect);
        }
    }

    DEBUG("Floating rect: (%d, %d) with %d x %d", nc->rect.x, nc->rect.y, nc->rect.width, nc->rect.height);

    render_container(nc);

    if (set_focus) {
        container_activate(con);
    }

    floating_set_hint_atom(nc, true);
//    ipc_send_window_event("floating", con);
    return true;
}

bool floating_reposition(GWMContainer *con, GWMRect newRect)
{
    if (!randr_output_containing_rect(newRect)) {
        ERROR("No output found at destination coordinates. Not repositioning.");
        return false;
    }

    con->rect = newRect;

    floating_maybe_reassign_ws(con);

    if (con->scratchpadState == SCRATCHPAD_FRESH) {
        con->scratchpadState = SCRATCHPAD_CHANGED;
    }

    tree_render();
    return true;
}

void floating_toggle_floating_mode(GWMContainer *con, bool automatic)
{
    if (con->type == CT_FLOATING_CON) {
        ERROR("Cannot toggle floating mode on con = %p because it is of type CT_FLOATING_CON.", con);
        return;
    }

    if (container_is_floating(con)) {
        DEBUG("already floating, re-setting to tiling");
        floating_disable(con);
        return;
    }

    floating_enable(con, automatic);
}

void floating_check_size(GWMContainer *floatingCon, bool preferHeight)
{
    const int floating_sane_min_height = 50;
    const int floating_sane_min_width = 75;
    GWMRect floating_sane_max_dimensions;
    GWMContainer* focused_con = container_descend_focused(floatingCon);

    DEBUG("deco_rect.height = %d", focused_con->decorationRect.height);
    GWMRect border_rect = container_border_style_rect(focused_con);
    border_rect.width = -border_rect.width;
    border_rect.height = -border_rect.height;

    border_rect.width += 2 * focused_con->borderWidth;
    border_rect.height += 2 * focused_con->borderWidth;

    DEBUG("floating_check_size, want min width %d, min height %d, border extra: w=%d, h=%d",
            floating_sane_min_width, floating_sane_min_height, border_rect.width, border_rect.height);

    GWMWindow* window = focused_con->window;
    if (window != NULL) {
        int min_width = (window->minWidth ? window->minWidth : window->baseWidth);
        int min_height = (window->minHeight ? window->minHeight : window->baseHeight);
        int base_width = (window->baseWidth ? window->baseWidth : window->minWidth);
        int base_height = (window->baseHeight ? window->baseHeight : window->minHeight);

        if (min_width) {
            floatingCon->rect.width -= border_rect.width;
            floatingCon->rect.width = MAX(floatingCon->rect.width, min_width);
            floatingCon->rect.width += border_rect.width;
        }

        if (min_height) {
            floatingCon->rect.height -= border_rect.height;
            floatingCon->rect.height = MAX(floatingCon->rect.height, min_height);
            floatingCon->rect.height += border_rect.height;
        }

        if (window->maxWidth) {
            floatingCon->rect.width -= border_rect.width;
            floatingCon->rect.width = MIN(floatingCon->rect.width, window->maxWidth);
            floatingCon->rect.width += border_rect.width;
        }

        if (window->maxHeight) {
            floatingCon->rect.height -= border_rect.height;
            floatingCon->rect.height = MIN(floatingCon->rect.height, window->maxHeight);
            floatingCon->rect.height += border_rect.height;
        }

        const double min_ar = window->minAspectRatio;
        const double max_ar = window->maxAspectRatio;
        if (floatingCon->fullScreenMode == CF_NONE && (min_ar > 0 || max_ar > 0)) {
            double width = floatingCon->rect.width - window->baseWidth - border_rect.width;
            double height = floatingCon->rect.height - window->baseHeight - border_rect.height;
            const double ar = (double)width / (double)height;
            double new_ar = -1;
            if (min_ar > 0 && ar < min_ar) {
                new_ar = min_ar;
            }
            else if (max_ar > 0 && ar > max_ar) {
                new_ar = max_ar;
            }
            if (new_ar > 0) {
                if (preferHeight) {
                    width = round(height * new_ar);
                    height = round(width / new_ar);
                } else {
                    height = round(width / new_ar);
                    width = round(height * new_ar);
                }
                floatingCon->rect.width = width + window->baseWidth + border_rect.width;
                floatingCon->rect.height = height + window->baseHeight + border_rect.height;
            }
        }

        if (window->heightInc && floatingCon->rect.height >= base_height + border_rect.height) {
            floatingCon->rect.height -= base_height + border_rect.height;
            floatingCon->rect.height -= floatingCon->rect.height % window->heightInc;
            floatingCon->rect.height += base_height + border_rect.height;
        }

        if (window->widthInc && floatingCon->rect.width >= base_width + border_rect.width) {
            floatingCon->rect.width -= base_width + border_rect.width;
            floatingCon->rect.width -= floatingCon->rect.width % window->widthInc;
            floatingCon->rect.width += base_width + border_rect.width;
        }
    }

    if (gConfig.floatingMinimumHeight != -1) {
        floatingCon->rect.height -= border_rect.height;
        if (gConfig.floatingMinimumHeight == 0) {
            floatingCon->rect.height = MAX(floatingCon->rect.height, floating_sane_min_height);
        }
        else {
            floatingCon->rect.height = MAX(floatingCon->rect.height, gConfig.floatingMinimumHeight);
        }
        floatingCon->rect.height += border_rect.height;
    }

    if (gConfig.floatingMinimumWidth != -1) {
        floatingCon->rect.width -= border_rect.width;
        if (gConfig.floatingMinimumWidth == 0) {
            floatingCon->rect.width = MAX(floatingCon->rect.width, floating_sane_min_width);
        }
        else {
            floatingCon->rect.width = MAX(floatingCon->rect.width, gConfig.floatingMinimumWidth);
        }
        floatingCon->rect.width += border_rect.width;
    }

    floating_sane_max_dimensions = total_outputs_dimensions();
    if (gConfig.floatingMaximumHeight != -1) {
        floatingCon->rect.height -= border_rect.height;
        if (gConfig.floatingMaximumHeight == 0) {
            floatingCon->rect.height = MIN(floatingCon->rect.height, floating_sane_max_dimensions.height);
        }
        else {
            floatingCon->rect.height = MIN(floatingCon->rect.height, gConfig.floatingMaximumHeight);
        }
        floatingCon->rect.height += border_rect.height;
    }

    if (gConfig.floatingMaximumWidth != -1) {
        floatingCon->rect.width -= border_rect.width;
        if (gConfig.floatingMaximumWidth == 0) {
            floatingCon->rect.width = MIN(floatingCon->rect.width, floating_sane_max_dimensions.width);
        }
        else {
            floatingCon->rect.width = MIN(floatingCon->rect.width, gConfig.floatingMaximumWidth);
        }
        floatingCon->rect.width += border_rect.width;
    }
}

void floating_resize(GWMContainer *floatingCon, uint32_t x, uint32_t y)
{
    DEBUG("floating resize to %dx%d px", x, y);
    GWMRect *rect = &floatingCon->rect;
    GWMContainer* focused_con = container_descend_focused(floatingCon);
    if (focused_con->window == NULL) {
        DEBUG("No window is focused. Not resizing.");
        return;
    }
    int wi = focused_con->window->widthInc;
    int hi = focused_con->window->heightInc;
    bool prefer_height = (rect->width == x);
    rect->width = x;
    rect->height = y;
    if (wi) {
        rect->width += (wi - 1 - rect->width) % wi;
    }
    if (hi) {
        rect->height += (hi - 1 - rect->height) % hi;
    }

    floating_check_size(floatingCon, prefer_height);

    if (floatingCon->scratchpadState == SCRATCHPAD_FRESH) {
        floatingCon->scratchpadState = SCRATCHPAD_CHANGED;
    }
}

void floating_fix_coordinates(GWMContainer *con, GWMRect *oldRect, GWMRect *newRect)
{
    DEBUG("Fixing coordinates of floating window %p (rect (%d, %d), %d x %d)",
            con, con->rect.x, con->rect.y, con->rect.width, con->rect.height);
    DEBUG("old_rect = (%d, %d), %d x %d", oldRect->x, oldRect->y, oldRect->width, oldRect->height);
    DEBUG("new_rect = (%d, %d), %d x %d", newRect->x, newRect->y, newRect->width, newRect->height);
    int32_t rel_x = con->rect.x - oldRect->x + (int32_t)(con->rect.width / 2);
    int32_t rel_y = con->rect.y - oldRect->y + (int32_t)(con->rect.height / 2);
    DEBUG("rel_x = %d, rel_y = %d, fraction_x = %f, fraction_y = %f, output->w = %d, output->h = %d",
            rel_x, rel_y, (double)rel_x / oldRect->width, (double)rel_y / oldRect->height,
            oldRect->width, oldRect->height);
    con->rect.x = (int32_t)newRect->x + (double)(rel_x * (int32_t)newRect->width) / (int32_t)oldRect->width - (int32_t)(con->rect.width / 2);
    con->rect.y = (int32_t)newRect->y + (double)(rel_y * (int32_t)newRect->height) / (int32_t)oldRect->height - (int32_t)(con->rect.height / 2);
    DEBUG("Resulting coordinates: x = %d, y = %d", con->rect.x, con->rect.y);
}

void floating_drag_window(GWMContainer *con, const xcb_button_press_event_t *event, bool useThreshold)
{
    DEBUG("floating_drag_window");

    tree_render();

    GWMRect initial_rect = con->rect;

    GWMDragResult drag_result = drag_pointer(con, event, XCB_NONE, CURSOR_MOVE, useThreshold, dragWindowCB, NULL);

    if (!container_exists(con)) {
        DEBUG("The container has been closed in the meantime.");
        return;
    }

    if (drag_result == DRAG_REVERT) {
        floating_reposition(con, initial_rect);
        return;
    }

    if (con->scratchpadState == SCRATCHPAD_FRESH) {
        con->scratchpadState = SCRATCHPAD_CHANGED;
    }

    tree_render();
}

void floating_resize_window(GWMContainer *con, bool proportional, const xcb_button_press_event_t *event)
{
    DEBUG("floating_resize_window");

    tree_render();
    GWMBorder corner = 0;

    if (event->event_x <= (int16_t)(con->rect.width / 2)) {
        corner |= BORDER_LEFT;
    }
    else {
        corner |= BORDER_RIGHT;
    }

    int cursor = 0;
    if (event->event_y <= (int16_t)(con->rect.height / 2)) {
        corner |= BORDER_TOP;
        cursor = (corner & BORDER_LEFT) ? CURSOR_TOP_LEFT_CORNER : CURSOR_TOP_RIGHT_CORNER;
    }
    else {
        corner |= BORDER_BOTTOM;
        cursor = (corner & BORDER_LEFT) ? CURSOR_BOTTOM_LEFT_CORNER : CURSOR_BOTTOM_RIGHT_CORNER;
    }

    struct resize_window_callback_params params = {corner, proportional};

    GWMRect initial_rect = con->rect;

    GWMDragResult drag_result = drag_pointer(con, event, XCB_NONE, cursor, false, resizeWindowCB, &params);

    if (!container_exists(con)) {
        DEBUG("The container has been closed in the meantime.");
        return;
    }

    if (drag_result == DRAG_REVERT) {
        floating_reposition(con, initial_rect);
    }

    if (con->scratchpadState == SCRATCHPAD_FRESH) {
        con->scratchpadState = SCRATCHPAD_CHANGED;
    }
}

static GWMRect total_outputs_dimensions(void)
{
    if (TAILQ_EMPTY(&gOutputs)) {
        return (GWMRect){0, 0, gRootScreen->width_in_pixels, gRootScreen->height_in_pixels};
    }

    GWMOutput *output;
    GWMRect outputs_dimensions = {0, 0, 0, 0};
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        outputs_dimensions.height += output->rect.height;
        outputs_dimensions.width += output->rect.width;
    }
    return outputs_dimensions;
}

static void floating_set_hint_atom(GWMContainer* con, bool floating)
{
    if (!container_is_leaf(con)) {
        GWMContainer* child;
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            floating_set_hint_atom(child, floating);
        }
    }

    if (con->window == NULL) {
        return;
    }

    if (floating) {
        uint32_t val = 1;
        xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, con->window->id, A_GWM_FLOATING_WINDOW, XCB_ATOM_CARDINAL, 32, 1, &val);
    }
    else {
        xcb_delete_property(gConn, con->window->id, A_GWM_FLOATING_WINDOW);
    }

    xcb_flush(gConn);
}