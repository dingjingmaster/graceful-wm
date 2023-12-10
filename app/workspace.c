//
// Created by dingjing on 23-11-28.
//
#include "workspace.h"

#include "val.h"
#include "log.h"
#include "output.h"
#include "utils.h"
#include "randr.h"
#include "container.h"
#include "x.h"
#include "gaps.h"
#include "extend-wm-hints.h"
#include "tree.h"
#include "floating.h"


static bool get_urgency_flag(GWMContainer* con);
static void workspace_reassign_sticky(GWMContainer* con);
static void workspace_defer_update_urgent_hint_cb(EV_P_ ev_timer* w, int rEvents);
static GWMContainer* _get_sticky(GWMContainer* con, const char *stickyGroup, GWMContainer* exclude);


char* previous_workspace_name = NULL;
static char** binding_workspace_names = NULL;


static void _workspace_apply_default_orientation(GWMContainer* ws);


void workspace_update_urgent_flag(GWMContainer *ws)
{
    bool old_flag = ws->urgent;
    ws->urgent = get_urgency_flag(ws);
    DEBUG("Workspace urgency flag changed from %d to %d", old_flag, ws->urgent);

    if (old_flag != ws->urgent) {
//        ipc_send_workspace_event("urgent", ws, NULL);
    }
}

GWMContainer* workspace_encapsulate(GWMContainer* ws)
{
    if (TAILQ_EMPTY(&(ws->nodesHead))) {
        ERROR("Workspace %p / %s has no children to encapsulate\n", ws, ws->name);
        return NULL;
    }

    GWMContainer* new = container_new(NULL, NULL);
    new->parent = ws;
    new->layout = ws->layout;

    GWMContainer** focusOrder = container_get_focus_order(ws);

    DEBUG("Moving children of workspace %p / %s into container %p", ws, ws->name, new);
    GWMContainer* child;
    while (!TAILQ_EMPTY(&(ws->nodesHead))) {
        child = TAILQ_FIRST(&(ws->nodesHead));
        container_detach(child);
        container_attach(child, new, true);
    }

    container_set_focus_order(new, focusOrder);
    free(focusOrder);

    container_attach(new, ws, true);

    return new;
}

bool workspace_is_visible(GWMContainer *ws)
{
    GWMContainer* output = container_get_output(ws);
    if (output == NULL) {
        return false;
    }
    GWMContainer* fs = container_get_full_screen_con(output, CF_OUTPUT);

    return (fs == ws);
}

GWMContainer *workspace_next(void)
{
    GWMContainer* current = container_get_workspace(gFocused);
    GWMContainer* next = NULL, *first = NULL, *first_opposite = NULL;
    GWMContainer* output;

    if (current->workspaceNum == -1) {
        if ((next = TAILQ_NEXT(current, nodes)) != NULL) {
            return next;
        }
        bool found_current = false;
        TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
            if (container_is_internal(output)) {
                continue;
            }
            NODES_FOREACH (output_get_content(output)) {
                if (child->type != CT_WORKSPACE) {
                    continue;
                }
                if (!first) {
                    first = child;
                }
                if (!first_opposite || (child->workspaceNum != -1 && child->workspaceNum < first_opposite->workspaceNum)) {
                    first_opposite = child;
                }
                if (child == current) {
                    found_current = true;
                }
                else if (child->workspaceNum == -1 && found_current) {
                    next = child;
                    return next;
                }
            }
        }
    }
    else {
        TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
            if (container_is_internal(output)) {
                continue;
            }
            NODES_FOREACH (output_get_content(output)) {
                if (child->type != CT_WORKSPACE) {
                    continue;
                }
                if (!first || (child->workspaceNum != -1 && child->workspaceNum < first->workspaceNum)) {
                    first = child;
                }
                if (!first_opposite && child->workspaceNum == -1) {
                    first_opposite = child;
                }
                if (child->workspaceNum == -1) {
                    break;
                }
                if (current->workspaceNum < child->workspaceNum && (!next || child->workspaceNum < next->workspaceNum)) {
                    next = child;
                }
            }
        }
    }

    if (!next) {
        next = first_opposite ? first_opposite : first;
    }

    return next;
}

GWMContainer *workspace_prev(void)
{
    GWMContainer* current = container_get_workspace(gFocused);
    GWMContainer* prev = NULL, *first_opposite = NULL, *last = NULL;
    GWMContainer* output;

    if (current->workspaceNum == -1) {
        prev = TAILQ_PREV(current, nodesHead, nodes);
        if (prev && prev->workspaceNum != -1) {
            prev = NULL;
        }
        if (!prev) {
            bool found_current = false;
            TAILQ_FOREACH_REVERSE (output, &(gContainerRoot->nodesHead), nodesHead, nodes) {
                if (container_is_internal(output)) {
                    continue;
                }
                NODES_FOREACH_REVERSE (output_get_content(output)) {
                    if (child->type != CT_WORKSPACE) {
                        continue;
                    }
                    if (!last) {
                        last = child;
                    }
                    if (!first_opposite || (child->workspaceNum != -1 && child->workspaceNum > first_opposite->workspaceNum)) {
                        first_opposite = child;
                    }
                    if (child == current) {
                        found_current = true;
                    }
                    else if (child->workspaceNum == -1 && found_current) {
                        prev = child;
                        return prev;
                    }
                }
            }
        }
    }
    else {
        TAILQ_FOREACH_REVERSE (output, &(gContainerRoot->nodesHead), nodesHead, nodes) {
            if (container_is_internal(output)) {
                continue;
            }
            NODES_FOREACH_REVERSE (output_get_content(output)) {
                if (child->type != CT_WORKSPACE) {
                    continue;
                }
                if (!last || (child->workspaceNum != -1 && last->workspaceNum < child->workspaceNum)) {
                    last = child;
                }
                if (!first_opposite && child->workspaceNum == -1) {
                    first_opposite = child;
                }
                if (child->workspaceNum == -1) {
                    continue;
                }
                if (current->workspaceNum > child->workspaceNum && (!prev || child->workspaceNum > prev->workspaceNum)) {
                    prev = child;
                }
            }
        }
    }

    if (!prev) {
        prev = first_opposite ? first_opposite : last;
    }

    return prev;
}

void workspace_back_and_forth(void)
{
    if (!previous_workspace_name) {
        DEBUG("No previous workspace name set. Not switching.");
        return;
    }

    workspace_show_by_name(previous_workspace_name);
}

void workspace_show(GWMContainer* workspace)
{
    GWMContainer* current, *old = NULL;

    if (container_is_internal(workspace)) {
        return;
    }

    TAILQ_FOREACH (current, &(workspace->parent->nodesHead), nodes) {
        if (current->fullScreenMode == CF_OUTPUT) {
            old = current;
        }
        current->fullScreenMode = CF_NONE;
    }

    workspace->fullScreenMode = CF_OUTPUT;
    current = container_get_workspace(gFocused);
    if (workspace == current) {
        DEBUG("Not switching, already there.");
        return;
    }

    GWMContainer* old_focus = old ? container_descend_focused(old) : NULL;

    if (current && !container_is_internal(current)) {
        FREE(previous_workspace_name);
        previous_workspace_name = g_strdup(current->name);
        DEBUG("Setting previous_workspace_name = %s", previous_workspace_name);
    }

    workspace_reassign_sticky(workspace);

    DEBUG("switching to %p / %s", workspace, workspace->name);
    GWMContainer* next = container_descend_focused(workspace);
    GWMContainer* old_output = container_get_output(gFocused);

    if (next->urgent && (int)(1000) > 0) {
        next->urgent = false;
        container_focus(next);

        gFocused->urgent = true;
        workspace->urgent = true;

        if (gFocused->urgencyTimer == NULL) {
            DEBUG("Deferring reset of urgency flag of con %p on newly shown workspace %p", gFocused, workspace);
            gFocused->urgencyTimer = calloc(1, sizeof(struct ev_timer));
            ev_timer_init(gFocused->urgencyTimer, workspace_defer_update_urgent_hint_cb, 100, 1000);
            gFocused->urgencyTimer->data = gFocused;
            ev_timer_start(gMainLoop, gFocused->urgencyTimer);
        }
        else {
            DEBUG("Resetting urgency timer of con %p on workspace %p", gFocused, workspace);
            ev_timer_again(gMainLoop, gFocused->urgencyTimer);
        }
    }
    else {
        container_focus(next);
    }

//    ipc_send_workspace_event("focus", workspace, current);

    DEBUG("old = %p / %s\n", old, (old ? old->name : "(null)"));
    if (old && TAILQ_EMPTY(&(old->nodesHead)) && TAILQ_EMPTY(&(old->floatingHead))) {
        if (!workspace_is_visible(old)) {
            DEBUG("Closing old workspace (%p / %s), it is empty\n", old, old->name);
//            yajl_gen gen = ipc_marshal_workspace_event("empty", old, NULL);
//            tree_close_internal(old, DONT_KILL_WINDOW, false);
//
//            const unsigned char *payload;
//            ylength length;
//            y(get_buf, &payload, &length);
//            ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, (const char *)payload);
//
//            y(free);
//
//            /* Avoid calling output_push_sticky_windows later with a freed container. */
//            if (old == old_focus) {
//                old_focus = NULL;
//            }
//
//            ewmh_update_desktop_properties();
        }
    }

    workspace->fullScreenMode = CF_OUTPUT;
    DEBUG("focused now = %p / %s\n", gFocused, gFocused->name);

    GWMContainer* new_output = container_get_output(gFocused);
    if (old_output != new_output) {
        x_set_warp_to(&next->rect);
    }

    /* Update the EWMH hints */
    extend_wm_hint_update_current_desktop();

    /* Push any sticky windows to the now visible workspace. */
    output_push_sticky_windows(old_focus);
}

GWMContainer *workspace_get(const char *num)
{
    GWMContainer* workspace = workspace_get_existing_workspace_by_name(num);
    if (workspace) {
        return workspace;
    }

    INFO("Creating new workspace \"%s\"", num);

    const int parsed_num = util_ws_name_to_number(num);

    GWMContainer* output = workspace_get_assigned_output(num, parsed_num);
    if (!output) {
        output = container_get_output(gFocused);
    }

    workspace = container_new(NULL, NULL);

    g_autofree char *name = g_strdup_printf ("[graceful-wm container] workspace %s", num);
    x_set_name(workspace, name);

    FREE(workspace->name);
    workspace->name = g_strdup(num);
    workspace->workspaceLayout = L_DEFAULT;
    workspace->workspaceNum = parsed_num;
    workspace->type = CT_WORKSPACE;
    workspace->gaps = gaps_for_workspace(workspace);

    container_attach(workspace, output_get_content(output), false);
    _workspace_apply_default_orientation(workspace);

//    ipc_send_workspace_event("init", workspace, NULL);
//    ewmh_update_desktop_properties();
    extend_wm_hint_update_desktop_properties();

    return workspace;
}

void workspace_show_by_name(const char *num)
{
    workspace_show(workspace_get(num));
}

void workspace_extract_workspace_names_from_bindings(void)
{
    GWMBinding* bind = NULL;
    int n = 0;
    if (binding_workspace_names != NULL) {
        for (int i = 0; binding_workspace_names[i] != NULL; i++) {
            free(binding_workspace_names[i]);
        }
        FREE(binding_workspace_names);
    }
    TAILQ_FOREACH (bind, gBindings, bindings) {
        DEBUG("binding with command %s", bind->command);
        if (strlen(bind->command) < strlen("workspace ") || strncasecmp(bind->command, "workspace", strlen("workspace")) != 0) {
            continue;
        }
        DEBUG("relevant command = %s", bind->command);
        const char *target = bind->command + strlen("workspace ");
        while (*target == ' ' || *target == '\t') {
            target++;
        }
        if (strncasecmp(target, "next", strlen("next")) == 0
            || strncasecmp(target, "prev", strlen("prev")) == 0
            || strncasecmp(target, "next_on_output", strlen("next_on_output")) == 0
            || strncasecmp(target, "prev_on_output", strlen("prev_on_output")) == 0
            || strncasecmp(target, "back_and_forth", strlen("back_and_forth")) == 0
            || strncasecmp(target, "current", strlen("current")) == 0) {
            continue;
        }
        if (strncasecmp(target, "--no-auto-back-and-forth", strlen("--no-auto-back-and-forth")) == 0) {
            target += strlen("--no-auto-back-and-forth");
            while (*target == ' ' || *target == '\t') {
                target++;
            }
        }
        if (strncasecmp(target, "number", strlen("number")) == 0) {
            target += strlen("number");
            while (*target == ' ' || *target == '\t')
                target++;
        }
        char *target_name = util_parse_string(&target, false);
        if (target_name == NULL) {
            continue;
        }
        if (strncasecmp(target_name, "__", strlen("__")) == 0) {
            INFO("Cannot create workspace \"%s\". Names starting with __ are i3-internal.", target);
            free(target_name);
            continue;
        }
        DEBUG("Saving workspace name \"%s\"", target_name);

        binding_workspace_names = realloc(binding_workspace_names, ++n * sizeof(char *));
        binding_workspace_names[n - 1] = target_name;
    }
    binding_workspace_names = realloc(binding_workspace_names, ++n * sizeof(char *));
    binding_workspace_names[n - 1] = NULL;
}

GWMContainer *workspace_next_on_output(void)
{
    GWMContainer* next = NULL;
    GWMContainer* output = container_get_output(gFocused);
    GWMContainer* current = container_get_workspace(gFocused);

    if (current->workspaceNum == -1) {
        next = TAILQ_NEXT(current, nodes);
    }
    else {
        NODES_FOREACH (output_get_content(output)) {
            if (child->type != CT_WORKSPACE) {
                continue;
            }
            if (child->workspaceNum == -1) {
                break;
            }
            if (current->workspaceNum < child->workspaceNum && (!next || child->workspaceNum < next->workspaceNum)) {
                next = child;
            }
        }
    }

    if (!next) {
        bool found_current = false;
        NODES_FOREACH (output_get_content(output)) {
            if (child->type != CT_WORKSPACE) {
                continue;
            }
            if (child == current) {
                found_current = true;
            }
            else if (child->workspaceNum == -1 && (current->workspaceNum != -1 || found_current)) {
                next = child;
                goto workspace_next_on_output_end;
            }
        }
    }

    if (!next) {
        NODES_FOREACH (output_get_content(output)) {
            if (child->type != CT_WORKSPACE) {
                continue;
            }
            if (!next || (child->workspaceNum != -1 && child->workspaceNum < next->workspaceNum)) {
                next = child;
            }
        }
    }

workspace_next_on_output_end:

    return next;
}

GWMContainer *workspace_prev_on_output(void)
{
    GWMContainer* prev = NULL;
    GWMContainer* output = container_get_output(gFocused);
    GWMContainer* current = container_get_workspace(gFocused);
    DEBUG("output = %s", output->name);

    if (current->workspaceNum == -1) {
        prev = TAILQ_PREV(current, nodesHead, nodes);
        if (prev && prev->workspaceNum != -1) {
            prev = NULL;
        }
    }
    else {
        NODES_FOREACH_REVERSE (output_get_content(output)) {
            if (child->type != CT_WORKSPACE || child->workspaceNum == -1) {
                continue;
            }
            if (current->workspaceNum > child->workspaceNum
                && (!prev || child->workspaceNum > prev->workspaceNum)) {
                prev = child;
            }
        }
    }

    if (!prev) {
        bool found_current = false;
        NODES_FOREACH_REVERSE (output_get_content(output)) {
            if (child->type != CT_WORKSPACE) {
                continue;
            }
            if (child == current) {
                found_current = true;
            }
            else if (child->workspaceNum == -1 && (current->workspaceNum != -1 || found_current)) {
                prev = child;
                goto workspace_prev_on_output_end;
            }
        }
    }

    if (!prev) {
        NODES_FOREACH_REVERSE (output_get_content(output)) {
            if (child->type != CT_WORKSPACE) {
                continue;
            }
            if (!prev || child->workspaceNum > prev->workspaceNum) {
                prev = child;
            }
        }
    }

workspace_prev_on_output_end:
    return prev;
}

GWMContainer *workspace_back_and_forth_get(void)
{
    if (!previous_workspace_name) {
        DEBUG("No previous workspace name set.");
        return NULL;
    }

    return workspace_get(previous_workspace_name);
}

GWMContainer *workspace_attach_to(GWMContainer *ws)
{
    DEBUG("Attaching a window to workspace %p / %s\n", ws, ws->name);

    if (ws->workspaceLayout == L_DEFAULT) {
        DEBUG("Default layout, just attaching it to the workspace itself.\n");
        return ws;
    }

    DEBUG("Non-default layout, creating a new split container\n");
    /* 1: create a new split container */
    GWMContainer* new = container_new(NULL, NULL);
    new->parent = ws;

    /* 2: set the requested layout on the split con */
    new->layout = ws->workspaceLayout;

    /* 4: attach the new split container to the workspace */
    DEBUG("Attaching new split %p to workspace %p\n", new, ws);
    container_attach(new, ws, false);

    /* 5: fix the percentages */
    container_fix_percent(ws);

    return new;
}

GWMContainer *workspace_get_existing_workspace_by_num(int num)
{
    GWMContainer* output, *workspace = NULL;
    TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
        GREP_FIRST(workspace, output_get_content(output), child->workspaceNum == num);
    }

    return workspace;
}

void workspace_move_to_output(GWMContainer *ws, GWMOutput *output)
{
    DEBUG("Moving workspace %p / %s to output %p / \"%s\".\n", ws, ws->name, output, output_primary_name(output));

    GWMOutput *current_output = output_get_output_for_con(ws);
    GWMContainer* content = output_get_content(output->container);
    DEBUG("got output %p with content %p\n", output, content);

    if (ws->parent == content) {
        DEBUG("Nothing to do, workspace already there\n");
        return;
    }

    GWMContainer* previously_visible_ws = TAILQ_FIRST(&(content->focusHead));
    if (previously_visible_ws) {
        DEBUG("Previously visible workspace = %p / %s", previously_visible_ws, previously_visible_ws->name);
    }
    else {
        DEBUG("No previously visible workspace on output.");
    }

    bool workspace_was_visible = workspace_is_visible(ws);
    if (container_num_children(ws->parent) == 1) {
        DEBUG("Creating a new workspace to replace \"%s\" (last on its output).\n", ws->name);

        /* check if we can find a workspace assigned to this output */
        bool used_assignment = false;
        GWMWorkspaceAssignment *assignment;
        TAILQ_FOREACH (assignment, &gWorkspaceAssignments, wsAssignments) {
            if (!workspace_output_triggers_assignment(current_output, assignment)) {
                continue;
            }
            const int num = util_ws_name_to_number(assignment->name);
            const bool attached = (num == -1) ? workspace_get_existing_workspace_by_name(assignment->name) : workspace_get_existing_workspace_by_num(num);
            if (attached) {
                continue;
            }

            /* so create the workspace referenced to by this assignment */
            DEBUG("Creating workspace from assignment %s.\n", assignment->name);
            workspace_get(assignment->name);
            used_assignment = true;
            break;
        }

        if (!used_assignment) {
            workspace_create_workspace_on_output(current_output, ws->parent);
        }
    }

    DEBUG("Detaching\n");

    /* detach from the old output and attach to the new output */
    GWMContainer* old_content = ws->parent;
    container_detach(ws);
    if (workspace_was_visible) {
        GWMContainer* focus_ws = TAILQ_FIRST(&(old_content->focusHead));
        DEBUG("workspace was visible, focusing %p / %s now\n", focus_ws, focus_ws->name);
        workspace_show(focus_ws);
    }
    container_attach(ws, content, false);

    GWMContainer* floating_con;
    TAILQ_FOREACH (floating_con, &(ws->floatingHead), floatingWindows) {
        floating_fix_coordinates(floating_con, &(old_content->rect), &(content->rect));
    }

//    ipc_send_workspace_event("move", ws, NULL);
    if (workspace_was_visible) {
        /* Focus the moved workspace on the destination output. */
        workspace_show(ws);
    }

    extend_wm_hint_update_desktop_properties();

    if (!previously_visible_ws) {
        return;
    }

    TAILQ_FOREACH (ws, &(content->nodesHead), nodes) {
        if (ws != previously_visible_ws) {
            continue;
        }

        CALL(previously_visible_ws, onRemoveChild);
        break;
    }
}

GWMContainer* workspace_get_existing_workspace_by_name(const char *name)
{
    GWMContainer* output, *workspace = NULL;
    TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
        GREP_FIRST(workspace, output_get_content(output), !strcasecmp(child->name, name));
    }

    return workspace;
}

GWMContainer *workspace_get_assigned_output(const char *name, long parsed_num)
{
    GWMContainer* output = NULL;
    GWMWorkspaceAssignment *assignment;
    TAILQ_FOREACH (assignment, &gWorkspaceAssignments, wsAssignments) {
        if (assignment->output == NULL) {
            continue;
        }

        if (name && strcmp(assignment->name, name) == 0) {
            DEBUG ("Found workspace name=\"%s\" assignment to output \"%s\"\n", name, assignment->output);
            GWMOutput *assigned_by_name = randr_get_output_by_name(assignment->output, true);
            if (assigned_by_name) {
                return assigned_by_name->container;
            }
        }
        else if (!output && parsed_num != -1 && util_name_is_digits(assignment->name) && util_ws_name_to_number(assignment->name) == parsed_num) {
            DEBUG("Found workspace number=%ld assignment to output \"%s\"", parsed_num, assignment->output);
            GWMOutput *assigned_by_num = randr_get_output_by_name(assignment->output, true);
            if (assigned_by_num) {
                output = assigned_by_num->container;
            }
        }
    }

    return output;
}

void workspace_ws_force_orientation(GWMContainer *ws, GWMOrientation orientation)
{
    /* 1: create a new split container */
    GWMContainer* split = container_new(NULL, NULL);
    split->parent = ws;

    /* 2: copy layout from workspace */
    split->layout = ws->layout;

    /* 3: move the existing cons of this workspace below the new con */
    GWMContainer** focusOrder = container_get_focus_order(ws);

    DEBUG("Moving cons\n");
    while (!TAILQ_EMPTY(&(ws->nodesHead))) {
        GWMContainer* child = TAILQ_FIRST(&(ws->nodesHead));
        container_detach(child);
        container_attach(child, split, true);
    }

    container_set_focus_order(split, focusOrder);
    free(focusOrder);

    /* 4: switch workspace layout */
    ws->layout = (orientation == HORIZON) ? L_SPLIT_H : L_SPLIT_V;
    DEBUG("split->layout = %d, ws->layout = %d\n", split->layout, ws->layout);

    /* 5: attach the new split container to the workspace */
    DEBUG("Attaching new split (%p) to ws (%p)\n", split, ws);
    container_attach(split, ws, false);

    /* 6: fix the percentages */
    container_fix_percent(ws);
}

GWMContainer *workspace_create_workspace_on_output(GWMOutput *output, GWMContainer *content)
{
    bool exists = true;
    GWMContainer* ws = container_new(NULL, NULL);
    ws->type = CT_WORKSPACE;

    for (int n = 0; binding_workspace_names[n] != NULL; n++) {
        char *target_name = binding_workspace_names[n];
        GWMContainer* assigned = workspace_get_assigned_output(target_name, -1);
        if (assigned && assigned != output->container) {
            continue;
        }

        const int num = util_ws_name_to_number(target_name);
        exists = (num == -1) ? workspace_get_existing_workspace_by_name(target_name) : workspace_get_existing_workspace_by_num(num);
        if (!exists) {
            ws->name = g_strdup(target_name);
            ws->workspaceNum = num;
            DEBUG("Used number %d for workspace with name %s\n", ws->workspaceNum, ws->name);
            break;
        }
    }

    if (exists) {
        DEBUG("Getting next unused workspace by number\n");
        int c = 0;
        while (exists) {
            c++;
            GWMContainer* assigned = workspace_get_assigned_output(NULL, c);
            exists = (workspace_get_existing_workspace_by_num(c) || (assigned && assigned != output->container));
            DEBUG("result for ws %d: exists = %d\n", c, exists);
        }
        ws->workspaceNum = c;
        ws->name = g_strdup_printf ("%d", c);
    }

    container_attach(ws, content, false);

    char* name = g_strdup_printf("[graceful-wm con] workspace %s", ws->name);
    x_set_name(ws, name);
    free(name);

    ws->gaps = gaps_for_workspace(ws);

    ws->fullScreenMode = CF_OUTPUT;

    ws->workspaceLayout = L_DEFAULT;
    _workspace_apply_default_orientation(ws);

//    ipc_send_workspace_event("init", ws, NULL);
    return ws;
}

bool workspace_output_triggers_assignment(GWMOutput *output, GWMWorkspaceAssignment *assignment)
{
    return 0;
}

static void _workspace_apply_default_orientation(GWMContainer* ws)
{
    ws->layout = L_SPLIT_H; //(config.default_orientation == HORIZ) ? L_SPLITH : L_SPLITV;
//    if (config.default_orientation == NO_ORIENTATION) {
//        Con *output = con_get_output(ws);
//        ws->layout = (output->rect.height > output->rect.width) ? L_SPLITV : L_SPLITH;
//        ws->rect = output->rect;
//        DLOG("Auto orientation. Workspace size set to (%d,%d), setting layout to %d.\n",
//             output->rect.width, output->rect.height, ws->layout);
//    } else {
//        ws->layout = (config.default_orientation == HORIZ) ? L_SPLITH : L_SPLITV;
//    }
}

static GWMContainer* _get_sticky(GWMContainer* con, const char *stickyGroup, GWMContainer* exclude)
{
    GWMContainer* current;

    TAILQ_FOREACH (current, &(con->nodesHead), nodes) {
        if (current != exclude
            && current->stickyGroup != NULL
            && current->window != NULL
            && strcmp(current->stickyGroup, stickyGroup) == 0) {
            return current;
        }

        GWMContainer* recurse = _get_sticky(current, stickyGroup, exclude);
        if (recurse != NULL) {
            return recurse;
        }
    }

    TAILQ_FOREACH (current, &(con->floatingHead), floatingWindows) {
        if (current != exclude
            && current->stickyGroup != NULL
            && current->window != NULL
            && strcmp(current->stickyGroup, stickyGroup) == 0) {
            return current;
        }

        GWMContainer* recurse = _get_sticky(current, stickyGroup, exclude);
        if (recurse != NULL) {
            return recurse;
        }
    }

    return NULL;
}

static void workspace_reassign_sticky(GWMContainer* con)
{
    GWMContainer* current;
    TAILQ_FOREACH (current, &(con->nodesHead), nodes) {
        if (current->stickyGroup == NULL) {
            workspace_reassign_sticky(current);
            continue;
        }

        DEBUG("Ah, this one is sticky: %s / %p", current->name, current);
        GWMContainer* output = container_get_output(current);
        GWMContainer* src = _get_sticky(output, current->stickyGroup, current);
        if (src == NULL) {
            DEBUG("No window found for this sticky group");
            workspace_reassign_sticky(current);
            continue;
        }

        x_move_window(src, current);
        current->window = src->window;
        current->mapped = true;
        src->window = NULL;
        src->mapped = false;

        x_reparent_child(current, src);

        DEBUG("re-assigned window from src %p to dest %p", src, current);
    }

    TAILQ_FOREACH (current, &(con->floatingHead), floatingWindows) {
        workspace_reassign_sticky(current);
    }
}

static void workspace_defer_update_urgent_hint_cb(EV_P_ ev_timer* w, int rEvents)
{
    GWMContainer* con = w->data;

    ev_timer_stop(gMainLoop, con->urgencyTimer);
    FREE(con->urgencyTimer);

    if (con->urgent) {
        DEBUG("Resetting urgency flag of con %p by timer\n", con);
        container_set_urgency(con, false);
        container_update_parents_urgency(con);
        workspace_update_urgent_flag(container_get_workspace(con));
//        ipc_send_window_event("urgent", con);
        tree_render();
    }
}

static bool get_urgency_flag(GWMContainer* con)
{
    GWMContainer* child;
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        if (child->urgent || get_urgency_flag(child)) {
            return true;
        }
    }

    TAILQ_FOREACH (child, &(con->floatingHead), floatingWindows) {
        if (child->urgent || get_urgency_flag(child)) {
            return true;
        }
    }

    return false;
}
