//
// Created by dingjing on 23-12-12.
//

#include "move.h"

#include "val.h"
#include "log.h"
#include "container.h"
#include "workspace.h"
#include "floating.h"
#include "utils.h"
#include "extend-wm-hints.h"
#include "output.h"
#include "randr.h"


static bool is_focused_descendant(GWMContainer* con, GWMContainer* ancestor);
static GWMContainer* lowest_common_ancestor(GWMContainer* a, GWMContainer* b);
static void move_to_output_directed(GWMContainer* con, GWMDirection direction);
static void attach_to_workspace(GWMContainer *con, GWMContainer *ws, GWMDirection direction);
static GWMContainer* child_containing_con_recursively(GWMContainer* ancestor, GWMContainer* con);


void move_tree_move(GWMContainer *con, GWMDirection direction)
{
    GWMPosition position;
    GWMContainer* target;

    DEBUG("Moving in direction %d", direction);

    /* 1: get the first parent with the same orientation */

    if (con->type == CT_WORKSPACE) {
        DEBUG("Not moving workspace");
        return;
    }

    if (con->fullScreenMode == CF_GLOBAL) {
        DEBUG("Not moving fullscreen global container");
        return;
    }

    if ((con->fullScreenMode == CF_OUTPUT) || (con->parent->type == CT_WORKSPACE && container_num_children(con->parent) == 1)) {
        move_to_output_directed(con, direction);
        return;
    }

    GWMOrientation o = util_orientation_from_direction(direction);
    GWMContainer* same_orientation = container_parent_with_orientation(con, o);
    do {
        if (!same_orientation) {
            if (container_is_floating(con)) {
                floating_disable(con);
                return;
            }
            if (container_inside_floating(con)) {
                DEBUG("Inside floating, moving to workspace");
                attach_to_workspace(con, container_get_workspace(con), direction);
                goto end;
            }
            DEBUG("Force-changing orientation");
            workspace_ws_force_orientation(container_get_workspace(con), o);
            same_orientation = container_parent_with_orientation(con, o);
        }

        /* easy case: the move is within this container */
        if (same_orientation == con->parent) {
            GWMContainer* swap = (direction == D_LEFT || direction == D_UP) ? TAILQ_PREV(con, nodesHead, nodes) : TAILQ_NEXT(con, nodes);
            if (swap) {
                if (!container_is_leaf(swap)) {
                    DEBUG("Moving into our bordering branch");
                    target = container_descend_direction(swap, direction);
                    position = (container_orientation(target->parent) != o || direction == D_UP || direction == D_LEFT ? AFTER : BEFORE);
                    move_insert_con_into(con, target, position);
                    goto end;
                }

                DEBUG("Swapping with sibling.");
                if (direction == D_LEFT || direction == D_UP) {
                    TAILQ_SWAP(swap, con, &(swap->parent->nodesHead), nodes);
                }
                else {
                    TAILQ_SWAP(con, swap, &(swap->parent->nodesHead), nodes);
                }

                /* redraw parents to ensure all parent split container titles are updated correctly */
                container_force_split_parents_redraw(con);

//                ipc_send_window_event("move", con);
                return;
            }

            if (con->parent == container_get_workspace(con)) {
                /* If we couldn't find a place to move it on this workspace, try
                 * to move it to a workspace on a different output */
                move_to_output_directed(con, direction);
                return;
            }

            /* If there was no con with which we could swap the current one,
             * search again, but starting one level higher. */
            same_orientation = container_parent_with_orientation(con->parent, o);
        }
    } while (same_orientation == NULL);

    /* this time, we have to move to another container */
    /* This is the container *above* 'con' (an ancestor of con) which is inside
     * 'same_orientation' */
    GWMContainer* above = con;
    while (above->parent != same_orientation) { above = above->parent; }

    /* Enforce the fullscreen focus restrictions. */
    if (!container_full_screen_permits_focusing(above->parent)) {
        DEBUG("Cannot move out of fullscreen container");
        return;
    }

    DEBUG("above = %p", above);

    GWMContainer* next = (direction == D_UP || direction == D_LEFT ? TAILQ_PREV(above, nodesHead, nodes) : TAILQ_NEXT(above, nodes));

    if (next && !container_is_leaf(next)) {
        DEBUG("Moving into the bordering branch of our adjacent container");
        target = container_descend_direction(next, direction);
        position = (container_orientation(target->parent) != o || direction == D_UP || direction == D_LEFT ? AFTER : BEFORE);
        move_insert_con_into(con, target, position);
    }
    else if (!next && con->parent->parent->type == CT_WORKSPACE && con->parent->layout != L_DEFAULT && container_num_children(con->parent) == 1) {
        /* Con is the lone child of a non-default layout container at the edge
         * of the workspace. Treat it as though the workspace is its parent
         * and move it to the next output. */
        DEBUG("Grandparent is workspace");
        move_to_output_directed(con, direction);
        return;
    } else {
        DEBUG("Moving into container above");
        position = (direction == D_UP || direction == D_LEFT ? BEFORE : AFTER);
        move_insert_con_into(con, above, position);
    }

end:
    /* force re-painting the indicators */
    FREE(con->decorationRenderParams);

//    ipc_send_window_event("move", con);
    tree_flatten(gContainerRoot);
    extend_wm_hint_update_wm_desktop();
}

void move_insert_con_into(GWMContainer *con, GWMContainer *target, GWMPosition position)
{
    GWMContainer* parent = target->parent;
    GWMContainer* old_parent = con->parent;

    GWMContainer* lca = lowest_common_ancestor(con, parent);
    if (lca == con) {
        ERROR("Container is being inserted into one of its descendants.");
        return;
    }

    GWMContainer* con_ancestor = child_containing_con_recursively(lca, con);
    GWMContainer* target_ancestor = child_containing_con_recursively(lca, target);
    bool moves_focus_from_ancestor = is_focused_descendant(con, con_ancestor);
    bool focus_before;

    if (con_ancestor == target_ancestor) {
        focus_before = moves_focus_from_ancestor;
    }
    else {
        GWMContainer* current;
        TAILQ_FOREACH (current, &(lca->focusHead), focused) {
            if (current == con_ancestor || current == target_ancestor) {
                break;
            }
        }
        focus_before = (current == con_ancestor);
    }

    if (moves_focus_from_ancestor && focus_before) {
        GWMContainer* place = TAILQ_PREV(con_ancestor, focusHead, focused);
        TAILQ_REMOVE(&(lca->focusHead), target_ancestor, focused);
        if (place) {
            TAILQ_INSERT_AFTER(&(lca->focusHead), place, target_ancestor, focused);
        }
        else {
            TAILQ_INSERT_HEAD(&(lca->focusHead), target_ancestor, focused);
        }
    }

    container_detach(con);
    container_fix_percent(con->parent);

    if (parent->type == CT_WORKSPACE) {
        GWMContainer* split = workspace_attach_to(parent);
        if (split != parent) {
            DEBUG("Got a new split con, using that one instead");
            con->parent = split;
            container_attach(con, split, false);
            DEBUG("attached");
            con->percent = 0.0;
            container_fix_percent(split);
            con = split;
            DEBUG("ok, continuing with con %p instead", con);
            container_detach(con);
        }
    }

    con->parent = parent;

    if (parent == lca) {
        if (focus_before) {
            TAILQ_INSERT_BEFORE(target, con, focused);
        }
        else {
            TAILQ_INSERT_AFTER(&(parent->focusHead), target, con, focused);
        }
    }
    else {
        if (focus_before) {
            TAILQ_INSERT_HEAD(&(parent->focusHead), con, focused);
        }
        else {
            TAILQ_INSERT_TAIL(&(parent->focusHead), con, focused);
        }
    }

    if (position == BEFORE) {
        TAILQ_INSERT_BEFORE(target, con, nodes);
    }
    else if (position == AFTER) {
        TAILQ_INSERT_AFTER(&(parent->nodesHead), target, con, nodes);
    }

    con->percent = 0.0;
    container_fix_percent(parent);

    CALL(old_parent, onRemoveChild);
}

static GWMContainer* lowest_common_ancestor(GWMContainer* a, GWMContainer* b)
{
    GWMContainer* parent_a = a;
    while (parent_a) {
        GWMContainer* parent_b = b;
        while (parent_b) {
            if (parent_a == parent_b) {
                return parent_a;
            }
            parent_b = parent_b->parent;
        }
        parent_a = parent_a->parent;
    }

    g_assert(false);
}


static GWMContainer* child_containing_con_recursively(GWMContainer* ancestor, GWMContainer* con)
{
    GWMContainer* child = con;
    while (child && child->parent != ancestor) {
        child = child->parent;
        g_assert(child->parent);
    }
    return child;
}

static bool is_focused_descendant(GWMContainer* con, GWMContainer* ancestor)
{
    GWMContainer* current = con;
    while (current != ancestor) {
        if (TAILQ_FIRST(&(current->parent->focusHead)) != current) {
            return false;
        }
        current = current->parent;
        g_assert(current->parent);
    }
    return true;
}

static void move_to_output_directed(GWMContainer* con, GWMDirection direction)
{
    GWMOutput *current_output = output_get_output_for_con(con);
    GWMOutput *output = randr_get_output_next(direction, current_output, CLOSEST_OUTPUT);

    if (!output) {
        DEBUG("No output in this direction found. Not moving.");
        return;
    }

    GWMContainer *ws = NULL;
    GREP_FIRST(ws, output_get_content(output->container), workspace_is_visible(child));

    if (!ws) {
        DEBUG("No workspace on output in this direction found. Not moving.");
        return;
    }

    GWMContainer* old_ws = container_get_workspace(con);
    const bool moves_focus = (gFocused == con);
    attach_to_workspace(con, ws, direction);
    if (moves_focus) {
        /* workspace_show will not correctly update the active workspace because
         * the focused container, con, is now a child of ws. To work around this
         * and still produce the correct workspace focus events (see
         * 517-regress-move-direction-ipc.t) we need to temporarily set focused
         * to the old workspace.
         *
         * The following happen:
         * 1. Focus con to push it on the top of the focus stack in its new
         * workspace
         * 2. Set focused to the old workspace to force workspace_show to
         * execute
         * 3. workspace_show will descend focus and target our con for
         * focusing. This also ensures that the mouse warps correctly.
         * See: #3518. */
        container_focus(con);
        gFocused = old_ws;
        workspace_show(ws);
        container_focus(con);
    }

    /* force re-painting the indicators */
    FREE(con->decorationRenderParams);

//    ipc_send_window_event("move", con);
    tree_flatten(gContainerRoot);
    extend_wm_hint_update_wm_desktop();
}

static void attach_to_workspace(GWMContainer *con, GWMContainer *ws, GWMDirection direction)
{
    container_detach(con);
    GWMContainer *old_parent = con->parent;
    con->parent = ws;

    if (direction == D_RIGHT || direction == D_DOWN) {
        TAILQ_INSERT_HEAD(&(ws->nodesHead), con, nodes);
    }
    else {
        TAILQ_INSERT_TAIL(&(ws->nodesHead), con, nodes);
    }

    TAILQ_INSERT_TAIL(&(ws->focusHead), con, focused);

    con->percent = 0.0;
    container_fix_percent(ws);

    container_fix_percent(old_parent);

    CALL(old_parent, onRemoveChild);
}
