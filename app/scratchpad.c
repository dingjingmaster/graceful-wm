//
// Created by dingjing on 23-11-24.
//

#include "scratchpad.h"

#include "val.h"
#include "log.h"
#include "workspace.h"
#include "floating.h"
#include "container.h"
#include "utils.h"


static int _lcm(const int m, const int n);
static int _gcd(const int m, const int n);



bool scratchpad_show(GWMContainer* con)
{
    DEBUG("should show scratchpad window %p", con);
    GWMContainer* __gwm_scratch = workspace_get("__gwm_scratch");
    GWMContainer* floating;

    if (!con && (floating = container_inside_floating(gFocused))
        && floating->scratchpadState != SCRATCHPAD_NONE) {
        DEBUG("Focused window is a scratchpad window, hiding it.");
        scratchpad_move(gFocused);
        return true;
    }

    GWMContainer* fs = gFocused;
    while (fs && fs->fullScreenMode == CF_NONE) {
        fs = fs->parent;
    }

    if (fs && fs->type != CT_WORKSPACE) {
        container_toggle_full_screen(fs, CF_OUTPUT);
    }

    GWMContainer* walk_con;
    GWMContainer* focused_ws = container_get_workspace(gFocused);
    TAILQ_FOREACH (walk_con, &(focused_ws->floatingHead), floatingWindows) {
        if (!con && (floating = container_inside_floating(walk_con))
            && floating->scratchpadState != SCRATCHPAD_NONE
            && floating != container_inside_floating(gFocused)) {
            DEBUG("Found an unfocused scratchpad window on this workspace");
            DEBUG("Focusing it: %p", walk_con);
            container_activate(container_descend_tiling_focused(walk_con));
            return true;
        }
    }

    focused_ws = container_get_workspace(gFocused);
    TAILQ_FOREACH (walk_con, &gAllContainer, allContainers) {
        GWMContainer* walk_ws = container_get_workspace(walk_con);
        if (!con
            && walk_ws
            && !container_is_internal(walk_ws)
            && focused_ws != walk_ws
            && (floating = container_inside_floating(walk_con))
            && floating->scratchpadState != SCRATCHPAD_NONE) {
            DEBUG("Found a visible scratchpad window on another workspace,");
            DEBUG("moving it to this workspace: con = %p", walk_con);
            container_move_to_workspace(floating, focused_ws, true, false, false);
            container_activate(container_descend_focused(walk_con));
            return true;
        }
    }

    if (con && con->parent->scratchpadState == SCRATCHPAD_NONE) {
        DEBUG("Window is not in the scratchpad, doing nothing.");
        return false;
    }

    GWMContainer* active = container_get_workspace(gFocused);
    GWMContainer* current = container_get_workspace(con);
    if (con
        && (floating = container_inside_floating(con))
        && floating->scratchpadState != SCRATCHPAD_NONE
        && current != __gwm_scratch) {
        if (current == active) {
            DEBUG("Window is a scratchpad window, hiding it.");
            scratchpad_move(con);
            return true;
        }
    }

    if (con == NULL) {
        con = TAILQ_FIRST(&(__gwm_scratch->floatingHead));
        if (!con) {
            DEBUG("You don't have any scratchpad windows yet.");
            DEBUG("Use 'move scratchpad' to move a window to the scratchpad.");
            return false;
        }
    }
    else {
        con = container_inside_floating(con);
    }

    container_move_to_workspace(con, active, true, false, false);

    if (con->scratchpadState == SCRATCHPAD_FRESH) {
        DEBUG("Adjusting size of this window.");
        GWMContainer* output = container_get_output(con);
        con->rect.width = output->rect.width * 0.5;
        con->rect.height = output->rect.height * 0.75;
        floating_check_size(con, false);
        floating_center(con, container_get_workspace(con)->rect);
    }

    if (current != active) {
        workspace_show(active);
    }

    container_activate(container_descend_focused(con));

    return true;
}

void scratchpad_fix_resolution(void)
{
    GWMContainer* __gwm_scratch = workspace_get("__gwm_scratch");
    GWMContainer* __gwm_output = container_get_output(__gwm_scratch);
    DEBUG("Current resolution: (%d, %d) %d x %d",
            __gwm_output->rect.x, __gwm_output->rect.y,
            __gwm_output->rect.width, __gwm_output->rect.height);
    GWMContainer* output;
    int new_width = -1, new_height = -1;
    TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
        if (output == __gwm_output) {
            continue;
        }
        DEBUG("output %s's resolution: (%d, %d) %d x %d",
                output->name, output->rect.x, output->rect.y,
                output->rect.width, output->rect.height);
        if (new_width == -1) {
            new_width = output->rect.width;
            new_height = output->rect.height;
        }
        else {
            new_width = _lcm(new_width, output->rect.width);
            new_height = _lcm(new_height, output->rect.height);
        }
    }

    GWMRect old_rect = __gwm_output->rect;

    DEBUG("new width = %d, new height = %d", new_width, new_height);
    __gwm_output->rect.width = new_width;
    __gwm_output->rect.height = new_height;

    GWMRect new_rect = __gwm_output->rect;

    if (util_rect_equals(new_rect, old_rect)) {
        DEBUG("Scratchpad size unchanged.");
        return;
    }

    DEBUG("Fixing coordinates of scratchpad windows");
    GWMContainer* con;
    TAILQ_FOREACH (con, &(__gwm_scratch->floatingHead), floatingWindows) {
        floating_fix_coordinates(con, &old_rect, &new_rect);
    }
}

void scratchpad_move(GWMContainer *con)
{
    if (con->type == CT_WORKSPACE) {
        DEBUG("'move scratchpad' used on a workspace \"%s\". Calling it recursively on all windows on this workspace.", con->name);
        GWMContainer* current;
        current = TAILQ_FIRST(&(con->focusHead));
        while (current) {
            GWMContainer* next = TAILQ_NEXT(current, focused);
            scratchpad_move(current);
            current = next;
        }
        return;
    }
    DEBUG("should move con %p to __gwm_scratch", con);

    GWMContainer* __gwm_scratch = workspace_get("__gwm_scratch");
    if (container_get_workspace(con) == __gwm_scratch) {
        DEBUG("This window is already on __gwm_scratch.");
        return;
    }

    container_disable_full_screen(con);
    GWMContainer* maybe_floating_con = container_inside_floating(con);
    if (maybe_floating_con == NULL) {
        floating_enable(con, false);
        con = con->parent;
    }
    else {
        con = maybe_floating_con;
    }

    container_move_to_workspace(con, __gwm_scratch, true, true, false);

    if (con->scratchpadState == SCRATCHPAD_NONE) {
        DEBUG("This window was never used as a scratchpad before.");
        if (con == maybe_floating_con) {
            DEBUG("It was in floating mode before, set scratchpad state to changed.");
            con->scratchpadState = SCRATCHPAD_CHANGED;
        }
        else {
            DEBUG("It was in tiling mode before, set scratchpad state to fresh.");
            con->scratchpadState = SCRATCHPAD_FRESH;
        }
    }
}

static int _gcd(const int m, const int n)
{
    if (n == 0)
        return m;
    return _gcd(n, (m % n));
}

static int _lcm(const int m, const int n)
{
    const int o = _gcd(m, n);
    return ((m * n) / o);
}