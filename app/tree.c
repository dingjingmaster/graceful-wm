//
// Created by dingjing on 23-11-24.
//

#include "tree.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "x.h"
#include "log.h"
#include "val.h"
#include "utils.h"
#include "randr.h"
#include "window.h"
#include "render.h"
#include "output.h"
#include "handlers.h"
#include "workspace.h"
#include "container.h"
#include "restore-layout.h"
#include "extend-wm-hints.h"


static GWMContainer* _create___gwm (void);
static void mark_unmapped(GWMContainer* con);


void tree_init(xcb_get_geometry_reply_t *geo)
{
    gContainerRoot = container_new(NULL, NULL);
    FREE(gContainerRoot->name);
    gContainerRoot->name = "root";
    gContainerRoot->type = CT_ROOT;
    gContainerRoot->layout = L_SPLIT_H;
    gContainerRoot->rect = (GWMRect) {
        geo->x,
        geo->y,
        geo->width,
        geo->height
    };

    _create___gwm();
}

void tree_render(void)
{
    if (NULL == gContainerRoot) {
        return;
    }

    DEBUG("-- BEGIN RENDERING --\n");
    /* Reset map state for all nodes in tree */
    /* TODO: a nicer method to walk all nodes would be good, maybe? */
    mark_unmapped(gContainerRoot);
    gContainerRoot->mapped = true;

    render_container(gContainerRoot);

    x_push_changes(gContainerRoot);
    DEBUG("-- END RENDERING --\n");
}

bool tree_level_up(void)
{
    /* Skip over floating containers and go directly to the grandparent
     * (which should always be a workspace) */
    if (gFocused->parent->type == CT_FLOATING_CON) {
        container_activate(gFocused->parent->parent);
        return true;
    }

    /* We can focus up to the workspace, but not any higher in the tree */
    if ((gFocused->parent->type != CT_CON
        && gFocused->parent->type != CT_WORKSPACE)
        || gFocused->type == CT_WORKSPACE) {
        ERROR(_("'focus parent': Focus is already on the workspace, cannot go higher than that."));
        return false;
    }
    container_activate(gFocused->parent);

    return true;
}

bool tree_level_down(void)
{
    GWMContainer* next = TAILQ_FIRST(&(gFocused->focusHead));
    if (next == TAILQ_END(&(focused->focus_head))) {
        DEBUG(_("cannot go down"));
        return false;
    }
    else if (next->type == CT_FLOATING_CON) {
        GWMContainer* child = TAILQ_FIRST(&(next->focusHead));
        if (child == TAILQ_END(&(next->focus_head))) {
            DEBUG(_("cannot go down"));
            return false;
        }
        else {
            next = TAILQ_FIRST(&(next->focusHead));
        }
    }

    container_activate(next);

    return true;
}

void tree_flatten(GWMContainer *child)
{

}

bool tree_next(GWMContainer *con, GWMDirection direction)
{
    return 0;
}

void tree_split(GWMContainer *con, GWMOrientation orientation)
{
    if (container_is_floating(con)) {
        DEBUG(_("Floating containers can't be split."));
        return;
    }

    if (con->type == CT_WORKSPACE) {
        if (container_num_children(con) < 2) {
            if (container_num_children(con) == 0) {
                DEBUG(_("Changing workspace_layout to L_DEFAULT"));
                con->workspaceLayout = L_DEFAULT;
            }
            DEBUG(_("Changing orientation of workspace"));
            con->layout = (orientation == HORIZON) ? L_SPLIT_H : L_SPLIT_V;
            return;
        }
        else {
            /* if there is more than one container on the workspace
             * move them into a new container and handle this instead */
            con = workspace_encapsulate(con);
        }
    }

    GWMContainer* parent = con->parent;

    /* Force re-rendering to make the indicator border visible. */
    container_force_split_parents_redraw(con);

    /* if we are in a container whose parent contains only one
     * child (its split functionality is unused so far), we just change the
     * orientation (more intuitive than splitting again) */
    if (container_num_children(parent) == 1 &&
        (parent->layout == L_SPLIT_H
            || parent->layout == L_SPLIT_V)) {
        parent->layout = (orientation == HORIZON) ? L_SPLIT_H : L_SPLIT_V;
        DEBUG(_("Just changing orientation of existing container"));
        return;
    }

    DEBUG(_("Splitting in orientation %d"), orientation);

    /* 2: replace it with a new Container */
    GWMContainer* newContainer = container_new(NULL, NULL);

    {
        g_queue_push_head(&(parent->nodesHead), newContainer);
        g_queue_push_head(&(parent->focusHead), newContainer);
    }

    newContainer->parent = parent;
    newContainer->layout = (orientation == HORIZON) ? L_SPLIT_H : L_SPLIT_V;

    /* 3: swap 'percent' (resize factor) */
    newContainer->percent = con->percent;
    con->percent = 0.0;

    /* 4: add it as a child to the new Container */
    container_attach(con, newContainer, false);
}

bool tree_restore(const char *path, xcb_get_geometry_reply_t *geometry)
{
    bool result = false;
    char *globbed = util_resolve_tilde(path);
    char *buf = NULL;

    if (!util_path_exists(globbed)) {
        INFO(_("%s does not exist, not restoring tree"), globbed);
        goto out;
    }

    ssize_t len;
    if ((len = util_slurp(globbed, &buf)) < 0) {
        /* slurp already logged an error. */
        goto out;
    }

    /* TODO: refactor the following */
    gContainerRoot = container_new(NULL, NULL);
    gContainerRoot->rect = (GWMRect){
        geometry->x,
        geometry->y,
        geometry->width,
        geometry->height
    };
    gFocused = gContainerRoot;

    tree_append_json(gFocused, buf, len, NULL);

    DEBUG(_("appended tree, using new root"));
    gContainerRoot = TAILQ_FIRST(&(gContainerRoot->nodesHead));
    if (!gContainerRoot) {
        goto out;
    }
    DEBUG(_("new root = %p"), gContainerRoot);
    GWMContainer* out = TAILQ_FIRST(&(gContainerRoot->nodesHead));
    DEBUG(_("out = %p"), out);
    GWMContainer* ws = TAILQ_FIRST(&(out->nodesHead));
    DEBUG(_("ws = %p"), ws);

    /* For in-place restarting into v4.2, we need to make sure the new
     * pseudo-output __i3 is present. */
    if (strcmp(out->name, "__gwm") != 0) {
        DEBUG(_("Adding pseudo-output __gwm during inplace restart"));
        GWMContainer* __gwm = _create___gwm();
        TAILQ_REMOVE(&(gContainerRoot->nodesHead), __gwm, nodes);
        TAILQ_INSERT_HEAD(&(gContainerRoot->nodesHead), __gwm, nodes);
    }

    restore_open_placeholder_windows(gContainerRoot);
    result = true;

out:
    free(globbed);
    free(buf);

    return result;
}

GWMContainer *tree_open_container(GWMContainer *con, GWMWindow *window)
{
    if (con == NULL) {
        /* every focusable Con has a parent (outputs have parent root) */
        con = gFocused->parent;
        /* If the parent is an output, we are on a workspace. In this case,
         * the new container needs to be opened as a leaf of the workspace. */
        if (con->parent->type == CT_OUTPUT && con->type != CT_DOCK_AREA) {
            con = gFocused;
        }

        /* If the currently focused container is a floating container, we
         * attach the new container to the currently focused spot in its
         * workspace. */
        if (con->type == CT_FLOATING_CON) {
            con = container_descend_tiling_focused(con->parent);
            if (con->type != CT_WORKSPACE) {
                con = con->parent;
            }
        }
        DEBUG("con = %p", con);
    }

    g_assert(con != NULL);

    /* 3. create the container and attach it to its parent */
    GWMContainer* newContainer = container_new (con, window);
    newContainer->layout = L_SPLIT_H;

    /* 4: re-calculate child->percent for each child */
    container_fix_percent(con);

    return newContainer;
}

GWMContainer *tree_get_tree_next_sibling(GWMContainer *con, GWMPosition direction)
{
    return NULL;
}

bool tree_close_internal(GWMContainer *con, GWMKillWindow killWindow, bool doNotKillParent)
{
    GWMContainer* parent = con->parent;

    /* remove the urgency hint of the workspace (if set) */
    if (con->urgent) {
        container_set_urgency(con, false);
        container_update_parents_urgency(con);
        workspace_update_urgent_flag(container_get_workspace(con));
    }

    DEBUG(_("closing %p, kill_window = %d"), con, killWindow);
    GWMContainer* child, *nextChild;
    bool abort_kill = false;
    /* We cannot use TAILQ_FOREACH because the children get deleted
     * in their parent’s nodes_head */
    for (child = TAILQ_FIRST(&(con->nodesHead)); child;) {
        nextChild = TAILQ_NEXT(child, nodes);
        DEBUG(_("killing child=%p"), child);
        if (!tree_close_internal(child, killWindow, true)) {
            abort_kill = true;
        }
        child = nextChild;
    }

    if (abort_kill) {
        DEBUG(_("One of the children could not be killed immediately (WM_DELETE sent), aborting."));
        return false;
    }

    if (con->window != NULL) {
        if (killWindow != KILL_WINDOW_DO_NOT) {
            x_window_kill(con->window->id, killWindow);
            return false;
        }
        else {
            xcb_void_cookie_t cookie;
            /* Ignore any further events by clearing the event mask,
             * unmap the window,
             * then reparent it to the root window. */
            xcb_change_window_attributes(gConn, con->window->id, XCB_CW_EVENT_MASK, (uint32_t[]){XCB_NONE});
            xcb_unmap_window(gConn, con->window->id);
            cookie = xcb_reparent_window(gConn, con->window->id, gRoot, con->rect.x, con->rect.y);

            /* Ignore X11 errors for the ReparentWindow request.
             * X11 Errors are returned when the window was already destroyed */
            handler_add_ignore_event(cookie.sequence, 0);

            /* We are no longer handling this window, thus set WM_STATE to
             * WM_STATE_WITHDRAWN (see ICCCM 4.1.3.1) */
            long data[] = {XCB_ICCCM_WM_STATE_WITHDRAWN, XCB_NONE};
            cookie = xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, con->window->id, A_WM_STATE, A_WM_STATE, 32, 2, data);

            /* Remove the window from the save set. All windows in the save set
             * will be mapped when i3 closes its connection (e.g. when
             * restarting). This is not what we want, since some apps keep
             * unmapped windows around and don’t expect them to suddenly be
             * mapped. See https://bugs.i3wm.org/1617 */
            xcb_change_save_set(gConn, XCB_SET_MODE_DELETE, con->window->id);

            /* Stop receiving ShapeNotify events. */
            if (gShapeSupported) {
                xcb_shape_select_input(gConn, con->window->id, false);
            }

            /* Ignore X11 errors for the ReparentWindow request.
             * X11 Errors are returned when the window was already destroyed */
            handler_add_ignore_event(cookie.sequence, 0);
        }
//        ipc_send_window_event("close", con);
        window_free(con->window);
        con->window = NULL;
    }

    GWMContainer* ws = container_get_workspace(con);

    /* Figure out which container to focus next before detaching 'con'. */
    GWMContainer* next = (con == gFocused) ? container_next_focused(con) : NULL;
    DEBUG("next = %p, focused = %p\n", next, gFocused);

    /* Detach the container so that it will not be rendered anymore. */
    container_detach(con);

    /* disable urgency timer, if needed */
    if (con->urgencyTimer != NULL) {
        DEBUG(_("Removing urgency timer of con %p"), con);
        workspace_update_urgent_flag(ws);
        ev_timer_stop(gMainLoop, con->urgencyTimer);
        FREE(con->urgencyTimer);
    }

    if (con->type != CT_FLOATING_CON) {
        /* If the container is *not* floating, we might need to re-distribute
         * percentage values for the resized containers. */
        container_fix_percent(parent);
    }

    /* Render the tree so that the surrounding containers take up the space
     * which 'con' does no longer occupy. If we don’t render here, there will
     * be a gap in our containers and that could trigger an EnterNotify for an
     * underlying container, see ticket #660.
     *
     * Rendering has to be avoided when dont_kill_parent is set (when
     * tree_close_internal calls itself recursively) because the tree is in a
     * non-renderable state during that time. */
    if (!doNotKillParent) {
        tree_render();
    }

    /* kill the X11 part of this container */
    x_container_kill(con);

    if (ws == con) {
        DEBUG(_("Closing workspace container %s, updating EWMH atoms"), ws->name);
        extend_wm_hint_update_desktop_properties();
    }

    container_free(con);

    if (next) {
        container_activate(next);
    }
    else {
        DEBUG(_("not changing focus, the container was not focused before"));
    }

    /* check if the parent container is empty now and close it */
    if (!doNotKillParent) {
        CALL(parent, onRemoveChild);
    }

    return true;
}

void tree_append_json(GWMContainer *con, const char *buf, size_t len, char **errorMsg)
{

}

static GWMContainer* _create___gwm (void)
{
    GWMContainer* __gwm = container_new(gContainerRoot, NULL);
    FREE(__gwm->name);
    __gwm->name = g_strdup("__gwm");
    __gwm->type = CT_OUTPUT;
    __gwm->layout = L_OUTPUT;
    container_fix_percent(gContainerRoot);
    x_set_name(__gwm, "[gwm con] pseudo-output __gwm");
    /* For retaining the correct position/size of a scratchpad window, the
     * dimensions of the real outputs should be multiples of the __i3
     * pseudo-output. Ensuring that is the job of scratchpad_fix_resolution()
     * which gets called after this function and after detecting all the
     * outputs (or whenever an output changes). */
    __gwm->rect.width = 1280;
    __gwm->rect.height = 1024;

    /* Add a content container. */
    DEBUG(_("adding main content container"));
    GWMContainer* content = container_new(NULL, NULL);
    content->type = CT_CON;
    FREE(content->name);
    content->name = g_strdup("content");
    content->layout = L_SPLIT_H;

    x_set_name(content, "[i3 con] content __i3");
    container_attach(content, __gwm, false);

    /* Attach the __i3_scratch workspace. */
    GWMContainer *ws = container_new(NULL, NULL);
    ws->type = CT_WORKSPACE;
    ws->workspaceNum = -1;
    ws->name = g_strdup("__gwm_scratch");
    ws->layout = L_SPLIT_H;
    container_attach(ws, content, false);
    x_set_name(ws, "[gwm con] workspace __gwm_scratch");
    ws->fullScreenMode = CF_OUTPUT;

    return __gwm;
}

static void mark_unmapped(GWMContainer* con)
{
    GWMContainer* current = NULL;

    con->mapped = false;
    TAILQ_FOREACH (current, &(con->nodesHead), nodes) {
        mark_unmapped(current);
    }
    if (con->type == CT_WORKSPACE) {
        TAILQ_FOREACH (current, &(con->floatingHead), floatingWindows) {
            mark_unmapped(current);
        }
    }
}

static GWMContainer* get_tree_next_workspace(GWMContainer* con, GWMDirection direction)
{
    if (container_get_full_screen_con(con, CF_GLOBAL)) {
        DEBUG(_("Cannot change workspace while in global fullscreen mode."));
        return NULL;
    }

    const uint32_t x = con->rect.x + (con->rect.width / 2);
    const uint32_t y = con->rect.y + (con->rect.height / 2);
    GWMOutput* currentOutput = randr_get_output_containing(x, y);
    if (!currentOutput) {
        return NULL;
    }
    DEBUG(_("Current output is %s"), output_primary_name(currentOutput));

    GWMOutput *nextOutput = randr_get_output_next(direction, currentOutput, CLOSEST_OUTPUT);
    if (!nextOutput) {
        return NULL;
    }
    DEBUG(_("Next output is %s"), output_primary_name(nextOutput));

    GWMContainer* workspace = NULL;
    GREP_FIRST(workspace, output_get_content(nextOutput->container), workspace_is_visible(child));

    return workspace;
}