//
// Created by dingjing on 23-11-24.
//

#include "x.h"
#include "val.h"
#include "log.h"
#include "match.h"
#include "randr.h"
#include "output.h"
#include "floating.h"
#include "container.h"
#include "workspace.h"
#include "utils.h"
#include "extend-wm-hints.h"


static bool randr_query_outputs_15(void);
static void randr_query_outputs_14(void);
static void fallback_to_root_output(void);
static bool any_randr_output_active(void);
static void move_content(GWMContainer* con);
static GWMOutput *get_output_by_id(xcb_randr_output_t id);
GWMOutput *get_output_by_name(const char *name, bool requireActive);
static void output_change_mode(xcb_connection_t *conn, GWMOutput *output);
static void handle_output(xcb_connection_t *conn, xcb_randr_output_t id, xcb_randr_get_output_info_reply_t *output, xcb_timestamp_t cts, xcb_randr_get_screen_resources_current_reply_t *res);


static GWMOutput*       gsRootOutput = NULL;
static bool             gsHasRandr15 = false;


GWMOutput *randr_get_output_containing(unsigned int x, unsigned int y)
{
    GWMOutput *output;
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (!output->active) {
            continue;
        }
        DEBUG("comparing x=%d y=%d with x=%d and y=%d width %d height %d", x, y, output->rect.x, output->rect.y, output->rect.width, output->rect.height);
        if (x >= output->rect.x && x < (output->rect.x + output->rect.width)
            && y >= output->rect.y && y < (output->rect.y + output->rect.height)) {
            return output;
        }
    }

    return NULL;
}

GWMOutput *randr_get_first_output(void)
{
    GWMOutput *output, *result = NULL;

    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (output->active) {
            if (output->primary) {
                return output;
            }
            if (!result) {
                result = output;
            }
        }
    }

    if (result) {
        return result;
    }


    ERROR("No usable outputs available.");
    exit (-1);
}

void randr_init(int *eventBase, bool disableRandr15)
{
    const xcb_query_extension_reply_t *extreply;

    gsRootOutput = randr_create_root_output(gConn);
    TAILQ_INSERT_TAIL(&gOutputs, gsRootOutput, outputs);

    extreply = xcb_get_extension_data(gConn, &xcb_randr_id);
    if (!extreply->present) {
        DEBUG("RandR is not present, activating root output.");
        fallback_to_root_output();
        return;
    }

    xcb_generic_error_t *err;
    xcb_randr_query_version_reply_t *randr_version =
        xcb_randr_query_version_reply(gConn, xcb_randr_query_version(gConn, XCB_RANDR_MAJOR_VERSION, XCB_RANDR_MINOR_VERSION), &err);
    if (err != NULL) {
        ERROR("Could not query RandR version: X11 error code %d", err->error_code);
        FREE(err);
        fallback_to_root_output();
        return;
    }

    gsHasRandr15 = (randr_version->major_version >= 1) && (randr_version->minor_version >= 5) && !disableRandr15;

    FREE(randr_version);

    randr_query_outputs();

    if (eventBase != NULL) {
        *eventBase = extreply->first_event;
    }

    xcb_randr_select_input(gConn, gRoot, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE | XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE | XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE | XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);

    xcb_flush(gConn);
}

GWMOutput *randr_get_output_next(GWMDirection direction, GWMOutput *current, GWMOutputCloseFar closeFar)
{
    GWMRect *cur = &(current->rect), *other;
    GWMOutput *output, *best = NULL;
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (!output->active) {
            continue;
        }
        other = &(output->rect);

        if ((direction == D_RIGHT && other->x > cur->x) || (direction == D_LEFT && other->x < cur->x)) {
            if ((other->y + other->height) <= cur->y || (cur->y + cur->height) <= other->y) {
                continue;
            }
        }
        else if ((direction == D_DOWN && other->y > cur->y) || (direction == D_UP && other->y < cur->y)) {
            if ((other->x + other->width) <= cur->x || (cur->x + cur->width) <= other->x) {
                continue;
            }
        }
        else {
            continue;
        }

        if (!best) {
            best = output;
            continue;
        }

        if (closeFar == CLOSEST_OUTPUT) {
            if ((direction == D_RIGHT && other->x < best->rect.x)
                || (direction == D_LEFT && other->x > best->rect.x)
                || (direction == D_DOWN && other->y < best->rect.y)
                || (direction == D_UP && other->y > best->rect.y)) {
                best = output;
                continue;
            }
        }
        else {
            if ((direction == D_RIGHT && other->x > best->rect.x)
                || (direction == D_LEFT && other->x < best->rect.x)
                || (direction == D_DOWN && other->y > best->rect.y)
                || (direction == D_UP && other->y < best->rect.y)) {
                best = output;
                continue;
            }
        }
    }

    DEBUG("current = %s, best = %s", output_primary_name(current), (best ? output_primary_name(best) : "NULL"));

    return best;
}

GWMOutput *randr_get_output_next_wrap(GWMDirection direction, GWMOutput *current)
{
    GWMOutput *best = randr_get_output_next(direction, current, CLOSEST_OUTPUT);
    if (!best) {
        GWMDirection opposite;
        if (direction == D_RIGHT) {
            opposite = D_LEFT;
        }
        else if (direction == D_LEFT) {
            opposite = D_RIGHT;
        }
        else if (direction == D_DOWN) {
            opposite = D_UP;
        }
        else {
            opposite = D_DOWN;
        }
        best = randr_get_output_next(opposite, current, FARTHEST_OUTPUT);
    }
    if (!best) {
        best = current;
    }

    DEBUG("current = %s, best = %s", output_primary_name(current), output_primary_name(best));

    return best;
}

GWMOutput *randr_get_output_by_name(const char *name, bool requireActive)
{
    return NULL;
}

GWMOutput *randr_create_root_output(xcb_connection_t *conn)
{
    GWMOutput *s = g_malloc0(sizeof(GWMOutput));

    s->active = false;
    s->rect.x = 0;
    s->rect.y = 0;
    s->rect.width = gRootScreen->width_in_pixels;
    s->rect.height = gRootScreen->height_in_pixels;

    GWMOutputName* outputName = g_malloc0(sizeof(GWMOutputName));
    outputName->name = "xroot-0";
    SLIST_INIT(&s->namesHead);
    SLIST_INSERT_HEAD(&s->namesHead, outputName, names);

    return s;
}

GWMOutput *randr_get_output_with_dimensions(GWMRect rect)
{
    GWMOutput *output;
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (!output->active) {
            continue;
        }
        DEBUG("comparing x=%d y=%d %dx%d with x=%d and y=%d %dx%d",
                rect.x, rect.y, rect.width, rect.height,
                output->rect.x, output->rect.y, output->rect.width, output->rect.height);
        if (rect.x == output->rect.x && rect.width == output->rect.width
            && rect.y == output->rect.y && rect.height == output->rect.height) {
            return output;
        }
    }

    return NULL;
}

GWMOutput *randr_output_containing_rect(GWMRect rect)
{
    GWMOutput *output;
    int lx = rect.x, uy = rect.y;
    int rx = rect.x + rect.width, by = rect.y + rect.height;
    long max_area = 0;
    GWMOutput *result = NULL;
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (!output->active) {
            continue;
        }
        int lx_o = (int)output->rect.x, uy_o = (int)output->rect.y;
        int rx_o = (int)(output->rect.x + output->rect.width), by_o = (int)(output->rect.y + output->rect.height);
        DEBUG("comparing x=%d y=%d with x=%d and y=%d width %d height %d",
                rect.x, rect.y, output->rect.x, output->rect.y, output->rect.width, output->rect.height);
        int left = MAX(lx, lx_o);
        int right = MIN(rx, rx_o);
        int bottom = MIN(by, by_o);
        int top = MAX(uy, uy_o);
        if (left < right && bottom > top) {
            long area = (right - left) * (bottom - top);
            if (area > max_area) {
                result = output;
            }
        }
    }

    return result;
}

GWMOutput *randr_get_output_from_rect(GWMRect rect)
{
    unsigned int mid_x = rect.x + rect.width / 2;
    unsigned int mid_y = rect.y + rect.height / 2;
    GWMOutput *output = randr_get_output_containing(mid_x, mid_y);

    return output ? output : randr_output_containing_rect(rect);
}

void randr_output_init_container(GWMOutput *output)
{
    GWMContainer *con = NULL, *current;
    bool reused = false;

    DEBUG("init_con for output %s", output_primary_name(output));

    TAILQ_FOREACH (current, &(gContainerRoot->nodesHead), nodes) {
        if (strcmp(current->name, output_primary_name(output)) != 0) {
            continue;
        }

        con = current;
        reused = true;
        DEBUG("Using existing con %p / %s", con, con->name);
        break;
    }

    if (con == NULL) {
        con = container_new(gContainerRoot, NULL);
        FREE(con->name);
        con->name = g_strdup(output_primary_name(output));
        con->type = CT_OUTPUT;
        con->layout = L_OUTPUT;
        container_fix_percent(gContainerRoot);
    }
    con->rect = output->rect;
    output->container = con;

    char *name = g_strdup_printf("[gwm con] output %s", con->name);
    x_set_name(con, name);
    FREE(name);

    if (reused) {
        DEBUG("Not adding workspace, this was a reused con");
        return;
    }

    DEBUG("Changing layout, adding top/bottom dock area");
    GWMContainer *topdock = container_new(NULL, NULL);
    topdock->type = CT_DOCK_AREA;
    topdock->layout = L_DOCK_AREA;
    GWMMatch *match = g_malloc0(sizeof(GWMMatch));
    match_init(match);
    match->dock = M_DOCK_TOP;
    match->insertWhere = M_BELOW;
    TAILQ_INSERT_TAIL(&(topdock->swallowHead), match, matches);

    FREE(topdock->name);
    topdock->name = g_strdup("topdock");

    name = g_strdup_printf (&name, "[gwm con] top dock area %s", con->name);
    x_set_name(topdock, name);
    FREE(name);
    DEBUG("attaching");
    container_attach(topdock, con, false);

    DEBUG("adding main content container");
    GWMContainer* content = container_new(NULL, NULL);
    content->type = CT_CON;
    content->layout = L_SPLIT_H;
    FREE(content->name);
    content->name = g_strdup("content");

    name = g_strdup_printf("[gwm con] content %s", con->name);
    x_set_name(content, name);
    FREE(name);
    container_attach(content, con, false);

    /* bottom dock container */
    GWMContainer* bottomdock = container_new(NULL, NULL);
    bottomdock->type = CT_DOCK_AREA;
    bottomdock->layout = L_DOCK_AREA;
    /* this container swallows dock clients */
    match = g_malloc0(sizeof(GWMMatch));
    match_init(match);
    match->dock = M_DOCK_BOTTOM;
    match->insertWhere = M_BELOW;
    TAILQ_INSERT_TAIL(&(bottomdock->swallowHead), match, matches);

    FREE(bottomdock->name);
    bottomdock->name = g_strdup("bottomdock");

    name = g_strdup_printf("[gwm con] bottom dock area %s", con->name);
    x_set_name(bottomdock, name);
    FREE(name);
    DEBUG("attaching");
    container_attach(bottomdock, con, false);

    /* Change focus to the content container */
    TAILQ_REMOVE(&(con->focusHead), content, focused);
    TAILQ_INSERT_HEAD(&(con->focusHead), content, focused);
}

void randr_init_ws_for_output(GWMOutput *output)
{
    GWMContainer* content = output_get_content(output->container);
    GWMContainer* previous_focus = container_get_workspace(gFocused);

    GWMContainer* workspace;
    TAILQ_FOREACH (workspace, &gAllContainer, allContainers) {
        if (workspace->type != CT_WORKSPACE || container_is_internal(workspace)) {
            continue;
        }

        GWMContainer* workspace_out = workspace_get_assigned_output(workspace->name, workspace->workspaceNum);

        if (output->container != workspace_out) {
            continue;
        }

        DEBUG("Moving workspace \"%s\" from output \"%s\" to \"%s\" due to assignment",
                workspace->name, output_primary_name(output_get_output_for_con(workspace)),
                output_primary_name(output));

        content->rect = output->container->rect;
        workspace_move_to_output(workspace, output);
    }

    gFocused = content;

    if (!TAILQ_EMPTY(&(content->nodesHead))) {
        GWMContainer* visible = NULL;
        GREP_FIRST(visible, content, child->fullScreenMode == CF_OUTPUT);
        if (!visible) {
            visible = TAILQ_FIRST(&(content->nodesHead));
            workspace_show(visible);
        }
        goto restore_focus;
    }

    GWMWorkspaceAssignment* assignment;
    TAILQ_FOREACH (assignment, &gWorkspaceAssignments, wsAssignments) {
        if (!workspace_output_triggers_assignment(output, assignment)) {
            continue;
        }

        DEBUG("Initializing first assigned workspace \"%s\" for output \"%s\"", assignment->name, assignment->output);
        workspace_show_by_name(assignment->name);
        goto restore_focus;
    }

    DEBUG("Now adding a workspace");
    workspace_show(workspace_create_workspace_on_output(output, content));

restore_focus:
    if (previous_focus) {
        workspace_show(previous_focus);
    }
}

void randr_disable_output(GWMOutput *output)
{
    g_assert(output->toBeDisabled);

    output->active = false;
    DEBUG("Output %s disabled, re-assigning workspaces/docks", output_primary_name(output));

    if (output->container != NULL) {
        GWMOutput* con = output->container;
        output->container = NULL;
        move_content(con);
    }

    output->toBeDisabled = false;
    output->changed = false;
}

void randr_query_outputs(void)
{
    GWMOutput* output, *other;

    if (!randr_query_outputs_15()) {
        randr_query_outputs_14();
    }

    if (any_randr_output_active()) {
        DEBUG("Active RandR output found. Disabling root output.");
        if (gsRootOutput && gsRootOutput->active) {
            gsRootOutput->toBeDisabled = true;
        }
    }
    else {
        DEBUG("No active RandR output found. Enabling root output.");
        gsRootOutput->active = true;
    }

    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (!output->active || output->toBeDisabled) {
            continue;
        }
        DEBUG("output %p / %s, position (%d, %d), checking for clones",
                output, output_primary_name(output), output->rect.x, output->rect.y);

        for (other = output; other != TAILQ_END(&outputs); other = TAILQ_NEXT(other, outputs)) {
            if (other == output || !other->active || other->toBeDisabled) {
                continue;
            }

            if (other->rect.x != output->rect.x || other->rect.y != output->rect.y) {
                continue;
            }

            DEBUG("output %p has the same position, its mode = %d x %d", other, other->rect.width, other->rect.height);
            uint32_t width = MIN(other->rect.width, output->rect.width);
            uint32_t height = MIN(other->rect.height, output->rect.height);

            const bool update_w = util_update_if_necessary(&(output->rect.width), width);
            const bool update_h = util_update_if_necessary(&(output->rect.height), height);
            if (update_w || update_h) {
                output->changed = true;
            }

            util_update_if_necessary(&(other->rect.width), width);
            util_update_if_necessary(&(other->rect.height), height);

            DEBUG("disabling output %p (%s)", other, output_primary_name(other));
            other->toBeDisabled = true;

            DEBUG("new output mode %d x %d, other mode %d x %d",
                    output->rect.width, output->rect.height,
                    other->rect.width, other->rect.height);
        }
    }

    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (output->active && output->container == NULL) {
            DEBUG("Need to initialize a Con for output %s", output_primary_name(output));
            randr_output_init_container(output);
            output->changed = false;
        }
    }

    GWMContainer* con;
    for (con = TAILQ_FIRST(&(gContainerRoot->nodesHead)); con;) {
        GWMContainer* next = TAILQ_NEXT(con, nodes);
        if (!container_is_internal(con) && get_output_by_name(con->name, true) == NULL) {
            DEBUG("No output %s found, moving its old content to first output", con->name);
            move_content(con);
        }
        con = next;
    }

    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (output->toBeDisabled) {
            randr_disable_output(output);
        }

        if (output->changed) {
            output_change_mode(gConn, output);
            output->changed = false;
        }
    }

    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (!output->active) {
            continue;
        }
        GWMContainer* content = output_get_content(output->container);
        if (!TAILQ_EMPTY(&(content->nodesHead))) {
            continue;
        }
        DEBUG("Should add ws for output %s", output_primary_name(output));
        randr_init_ws_for_output(output);
    }

    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (!output->primary || !output->container) {
            continue;
        }

        DEBUG("Focusing primary output %s", output_primary_name(output));
        GWMContainer* content = output_get_content(output->container);
        GWMContainer* ws = TAILQ_FIRST(&(content)->focusHead);
        workspace_show(ws);
    }

    extend_wm_hint_update_desktop_properties();
    tree_render();

    FREE(gPrimary);
}



static GWMOutput *get_output_by_id(xcb_randr_output_t id)
{
    GWMOutput *output;
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (output->id == id) {
            return output;
        }
    }

    return NULL;
}

GWMOutput *get_output_by_name(const char *name, bool requireActive)
{
    const bool get_primary = (strcasecmp("primary", name) == 0);
    const bool get_non_primary = (strcasecmp("nonprimary", name) == 0);

    GWMOutput *output;
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (requireActive && !output->active) {
            continue;
        }
        if (output->primary && get_primary) {
            return output;
        }
        if (!output->primary && get_non_primary) {
            return output;
        }
        GWMOutputName * output_name;
        SLIST_FOREACH (output_name, &output->namesHead, names) {
            if (strcasecmp(output_name->name, name) == 0) {
                return output;
            }
        }
    }

    return NULL;
}

static bool any_randr_output_active(void)
{
    GWMOutput *output;

    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (output != gsRootOutput && !output->toBeDisabled && output->active)
            return true;
    }

    return false;
}

static void output_change_mode(xcb_connection_t *conn, GWMOutput *output)
{
    DEBUG("Output mode changed, updating rect");
    g_assert(output->container != NULL);
    output->container->rect = output->rect;

    GWMContainer* content, *workspace, *child;

    /* Point content to the container of the workspaces */
    content = output_get_content(output->container);

    TAILQ_FOREACH (workspace, &(content->nodesHead), nodes) {
        TAILQ_FOREACH (child, &(workspace->floatingHead), floatingWindows) {
            floating_fix_coordinates(child, &(workspace->rect), &(output->container->rect));
        }
    }

    /*if (config.default_orientation == NO_ORIENTATION) */
    {
        TAILQ_FOREACH (workspace, &(content->nodesHead), nodes) {
            if (container_num_children(workspace) > 1) {
                continue;
            }

            workspace->layout = (output->rect.height > output->rect.width) ? L_SPLIT_V : L_SPLIT_H;
            DEBUG("Setting workspace [%d,%s]'s layout to %d.", workspace->workspaceNum, workspace->name, workspace->layout);
            if ((child = TAILQ_FIRST(&(workspace->nodesHead)))) {
                if (child->layout == L_SPLIT_V || child->layout == L_SPLIT_H) {
                    child->layout = workspace->layout;
                }
                DEBUG("Setting child [%d,%s]'s layout to %d.", child->workspaceNum, child->name, child->layout);
            }
        }
    }
}


static bool randr_query_outputs_15(void)
{
#if XCB_RANDR_MINOR_VERSION < 5
    return false;
#else
    if (!gsHasRandr15) {
        return false;
    }
    DEBUG("Querying outputs using RandR 1.5");
    xcb_generic_error_t *err;
    xcb_randr_get_monitors_reply_t *monitors = xcb_randr_get_monitors_reply(gConn, xcb_randr_get_monitors(gConn, gRoot, true), &err);
    if (err != NULL) {
        ERROR("Could not get RandR monitors: X11 error code %d", err->error_code);
        FREE(err);
        return false;
    }

    GWMOutput *output;
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (output != gsRootOutput) {
            output->toBeDisabled = true;
        }
    }

    DEBUG("%d RandR monitors found (timestamp %d)", xcb_randr_get_monitors_monitors_length(monitors), monitors->timestamp);
    xcb_randr_monitor_info_iterator_t iter;
    for (iter = xcb_randr_get_monitors_monitors_iterator(monitors); iter.rem; xcb_randr_monitor_info_next(&iter)) {
        const xcb_randr_monitor_info_t *monitor_info = iter.data;
        xcb_get_atom_name_reply_t *atom_reply = xcb_get_atom_name_reply(gConn, xcb_get_atom_name(gConn, monitor_info->name), &err);
        if (err != NULL) {
            ERROR("Could not get RandR monitor name: X11 error code %d", err->error_code);
            FREE(err);
            continue;
        }
        char *name = g_strdup_printf("%.*s", xcb_get_atom_name_name_length(atom_reply), xcb_get_atom_name_name(atom_reply));
        FREE(atom_reply);

        GWMOutput* newO = get_output_by_name(name, false);
        if (newO == NULL) {
            newO = g_malloc0(sizeof(GWMOutput));
            SLIST_INIT(&newO->namesHead);

            xcb_randr_output_t *randr_outputs = xcb_randr_monitor_info_outputs(monitor_info);
            int randr_output_len = xcb_randr_monitor_info_outputs_length(monitor_info);
            for (int i = 0; i < randr_output_len; i++) {
                xcb_randr_output_t randr_output = randr_outputs[i];
                xcb_randr_get_output_info_reply_t *info = xcb_randr_get_output_info_reply(gConn, xcb_randr_get_output_info(gConn, randr_output, monitors->timestamp), NULL);
                if (info != NULL && info->crtc != XCB_NONE) {
                    char *oname = g_strdup_printf("%.*s", xcb_randr_get_output_info_name_length(info), xcb_randr_get_output_info_name(info));
                    if (strcmp(name, oname) != 0) {
                        GWMOutputName* output_name = g_malloc(sizeof(GWMOutputName));
                        output_name->name = g_strdup(oname);
                        SLIST_INSERT_HEAD(&newO->namesHead, output_name, names);
                    }
                    else {
                        FREE(oname);
                    }
                }
                FREE(info);
            }

            /* Insert the monitor name last, so that it's used as the primary name */
            GWMOutputName* output_name = g_malloc0(sizeof(GWMOutputName));
            output_name->name = g_strdup(name);
            SLIST_INSERT_HEAD(&newO->namesHead, output_name, names);
            if (monitor_info->primary) {
                TAILQ_INSERT_HEAD(&gOutputs, newO, outputs);
            }
            else {
                TAILQ_INSERT_TAIL(&gOutputs, newO, outputs);
            }
        }
        newO->active = true;
        newO->toBeDisabled = false;
        newO->primary = monitor_info->primary;

        const bool update_x = util_update_if_necessary(&(newO->rect.x), monitor_info->x);
        const bool update_y = util_update_if_necessary(&(newO->rect.y), monitor_info->y);
        const bool update_w = util_update_if_necessary(&(newO->rect.width), monitor_info->width);
        const bool update_h = util_update_if_necessary(&(newO->rect.height), monitor_info->height);

        newO->changed = update_x || update_y || update_w || update_h;

        DEBUG("name %s, x %d, y %d, width %d px, height %d px, width %d mm, height %d mm, primary %d, automatic %d",
                name,
                monitor_info->x, monitor_info->y, monitor_info->width, monitor_info->height,
                monitor_info->width_in_millimeters, monitor_info->height_in_millimeters,
                monitor_info->primary, monitor_info->automatic);
        FREE(name);
    }
    FREE(monitors);
    return true;
#endif
}


static void handle_output(xcb_connection_t *conn, xcb_randr_output_t id, xcb_randr_get_output_info_reply_t *output, xcb_timestamp_t cts, xcb_randr_get_screen_resources_current_reply_t *res)
{
    xcb_randr_get_crtc_info_reply_t *crtc;

    GWMOutput* newO = get_output_by_id(id);
    bool existing = (newO != NULL);
    if (!existing) {
        newO = g_malloc0(sizeof(GWMOutput));
        SLIST_INIT(&newO->namesHead);
    }
    newO->id = id;
    newO->primary = (gPrimary && gPrimary->output == id);
    while (!SLIST_EMPTY(&newO->namesHead)) {
        FREE(SLIST_FIRST(&newO->namesHead)->name);
        struct output_name *old_head = SLIST_FIRST(&newO->namesHead);
        SLIST_REMOVE_HEAD(&newO->namesHead, names);
        FREE(old_head);
    }
    GWMOutputName* output_name = g_malloc0(sizeof(GWMOutputName));
    output_name->name = g_strdup_printf("%.*s", xcb_randr_get_output_info_name_length(output), xcb_randr_get_output_info_name(output));
    SLIST_INSERT_HEAD(&newO->namesHead, output_name, names);

    DEBUG("found output with name %s", output_primary_name(newO));

    if (output->crtc == XCB_NONE) {
        if (!existing) {
            if (newO->primary) {
                TAILQ_INSERT_HEAD(&gOutputs, newO, outputs);
            }
            else {
                TAILQ_INSERT_TAIL(&gOutputs, newO, outputs);
            }
        }
        else if (newO->active) {
            newO->toBeDisabled = true;
        }
        return;
    }

    xcb_randr_get_crtc_info_cookie_t icookie;
    icookie = xcb_randr_get_crtc_info(conn, output->crtc, cts);
    if ((crtc = xcb_randr_get_crtc_info_reply(conn, icookie, NULL)) == NULL) {
        DEBUG("Skipping output %s: could not get CRTC (%p)", output_primary_name(newO), crtc);
        FREE(newO);
        return;
    }

    const bool update_x = util_update_if_necessary(&(newO->rect.x), crtc->x);
    const bool update_y = util_update_if_necessary(&(newO->rect.y), crtc->y);
    const bool update_w = util_update_if_necessary(&(newO->rect.width), crtc->width);
    const bool update_h = util_update_if_necessary(&(newO->rect.height), crtc->height);
    const bool updated = update_x || update_y || update_w || update_h;
    FREE(crtc);
    newO->active = (newO->rect.width != 0 && newO->rect.height != 0);
    if (!newO->active) {
        DEBUG("width/height 0/0, disabling output");
        return;
    }

    DEBUG("mode: %dx%d+%d+%d", newO->rect.width, newO->rect.height, newO->rect.x, newO->rect.y);

    if (!updated || !existing) {
        if (!existing) {
            if (newO->primary) {
                TAILQ_INSERT_HEAD(&gOutputs, newO, outputs);
            }
            else {
                TAILQ_INSERT_TAIL(&gOutputs, newO, outputs);
            }
        }
        return;
    }

    newO->changed = true;
}

static void randr_query_outputs_14(void)
{
    DEBUG("Querying outputs using RandR ≤ 1.4");

    xcb_randr_get_screen_resources_current_cookie_t rcookie;
    rcookie = xcb_randr_get_screen_resources_current(gConn, gRoot);
    xcb_randr_get_output_primary_cookie_t pcookie;
    pcookie = xcb_randr_get_output_primary(gConn, gRoot);

    if ((gPrimary = xcb_randr_get_output_primary_reply(gConn, pcookie, NULL)) == NULL) {
        ERROR("Could not get RandR primary output");
    }
    else {
        DEBUG("primary output is %08x", gPrimary->output);
    }

    xcb_randr_get_screen_resources_current_reply_t *res = xcb_randr_get_screen_resources_current_reply(gConn, rcookie, NULL);
    if (res == NULL) {
        ERROR("Could not query screen resources.");
        return;
    }

    const xcb_timestamp_t cts = res->config_timestamp;

    const int len = xcb_randr_get_screen_resources_current_outputs_length(res);

    xcb_randr_output_t *randr_outputs = xcb_randr_get_screen_resources_current_outputs(res);

    xcb_randr_get_output_info_cookie_t ocookie[len];
    for (int i = 0; i < len; i++) {
        ocookie[i] = xcb_randr_get_output_info(gConn, randr_outputs[i], cts);
    }

    for (int i = 0; i < len; i++) {
        xcb_randr_get_output_info_reply_t *output;

        if ((output = xcb_randr_get_output_info_reply(gConn, ocookie[i], NULL)) == NULL) {
            continue;
        }

        handle_output(gConn, randr_outputs[i], output, cts, res);
        FREE(output);
    }

    FREE(res);
}


static void move_content(GWMContainer* con)
{
    GWMContainer* first = randr_get_first_output()->container;
    GWMContainer* first_content = output_get_content(first);

    GWMContainer* next = gFocused;

    GWMContainer *current;
    GWMContainer *old_content = output_get_content(con);
    while (!TAILQ_EMPTY(&(old_content->nodesHead))) {
        current = TAILQ_FIRST(&(old_content->nodesHead));
        if (current != next && TAILQ_EMPTY(&(current->focusHead))) {
            DEBUG("Getting rid of current = %p / %s (empty, unfocused)", current, current->name);
            tree_close_internal(current, KILL_WINDOW_DO_NOT, false);
            continue;
        }
        DEBUG("Detaching current = %p / %s", current, current->name);
        container_detach(current);
        DEBUG("Re-attaching current = %p / %s", current, current->name);
        container_attach(current, first_content, false);
        DEBUG("Fixing the coordinates of floating containers");
        GWMContainer* floating_con;
        TAILQ_FOREACH (floating_con, &(current->floatingHead), floatingWindows) {
            floating_fix_coordinates(floating_con, &(con->rect), &(first->rect));
        }
    }

    if (next) {
        DEBUG("now focusing next = %p", next);
        container_focus(next);
        workspace_show(container_get_workspace(next));
    }

    GWMContainer* child;
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        if (child->type != CT_DOCK_AREA) {
            continue;
        }
        DEBUG("Handling dock con %p", child);
        GWMContainer* dock;
        while (!TAILQ_EMPTY(&(child->nodesHead))) {
            dock = TAILQ_FIRST(&(child->nodesHead));
            GWMContainer* nc;
            GWMMatch *match;
            nc = container_for_window(first, dock->window, &match);
            DEBUG("Moving dock client %p to nc %p", dock, nc);
            container_detach(dock);
            DEBUG("Re-attaching");
            container_attach(dock, nc, false);
            DEBUG("Done");
        }
    }

    DEBUG("Destroying disappearing con %p", con);
    tree_close_internal(con, KILL_WINDOW_DO_NOT, true);
}

static void fallback_to_root_output(void)
{
    gsRootOutput->active = true;
    randr_output_init_container(gsRootOutput);
    randr_init_ws_for_output(gsRootOutput);
}