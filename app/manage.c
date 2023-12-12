//
// Created by dingjing on 23-11-27.
//

#include "manage.h"

#include "val.h"
#include "log.h"
#include "match.h"
#include "x.h"
#include "xcb.h"
#include "container.h"
#include "window.h"
#include "startup.h"
#include "randr.h"
#include "output.h"
#include "assignments.h"
#include "workspace.h"
#include "tree.h"
#include "extend-wm-hints.h"
#include "floating.h"
#include "render.h"
#include "draw-util.h"
#include "key-bindings.h"
#include "utils.h"


static void _remove_matches(GWMContainer* con);
static xcb_window_t _match_depth(GWMWindow* win, GWMContainer* con);



void manage_restore_geometry(void)
{
    DEBUG("Restoring geometry");

    GWMContainer* con;
    TAILQ_FOREACH (con, &gAllContainer, allContainers) {
        if (con->window) {
            DEBUG("Re-adding X11 border of %d px\n", con->borderWidth);
            con->windowRect.width += (2 * con->borderWidth);
            con->windowRect.height += (2 * con->borderWidth);
            xcb_gwm_set_window_rect(gConn, con->window->id, con->windowRect);
            DEBUG("placing window %08x at %d %d\n", con->window->id, con->rect.x, con->rect.y);
            xcb_reparent_window(gConn, con->window->id, gRoot, con->rect.x, con->rect.y);
        }
    }

    xcb_change_window_attributes(gConn, gRoot, XCB_CW_EVENT_MASK, (uint32_t[]){XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT});

    xcb_aux_sync(gConn);
}

void manage_existing_windows(xcb_window_t root)
{
    int i, len;
    xcb_window_t *children;
    xcb_query_tree_reply_t *reply;
    xcb_get_window_attributes_cookie_t *cookies;

    if ((reply = xcb_query_tree_reply(gConn, xcb_query_tree(gConn, root), 0)) == NULL) {
        return;
    }

    len = xcb_query_tree_children_length(reply);
    cookies = g_malloc0(len * sizeof(*cookies));

    children = xcb_query_tree_children(reply);
    for (i = 0; i < len; ++i) {
        cookies[i] = xcb_get_window_attributes(gConn, children[i]);
    }

    for (i = 0; i < len; ++i) {
        manage_window(children[i], cookies[i], true);
    }

    free(reply);
    free(cookies);
}

GWMContainer *manage_remanage_window(GWMContainer *con)
{
    return NULL;
}

void manage_window(xcb_window_t window, xcb_get_window_attributes_cookie_t cookie, bool needsToBeMapped)
{
    DEBUG("window 0x%08x", window);

    xcb_drawable_t d = {window};
    xcb_get_geometry_cookie_t geomc;
    xcb_get_geometry_reply_t *geom;
    xcb_get_window_attributes_reply_t *attr = NULL;

    xcb_get_property_cookie_t wm_type_cookie, strut_cookie, state_cookie,
        utf8_title_cookie, title_cookie,
        class_cookie, leader_cookie, transient_cookie,
        role_cookie, startup_id_cookie, wm_hints_cookie,
        wm_normal_hints_cookie, motif_wm_hints_cookie, wm_user_time_cookie, wm_desktop_cookie,
        wm_machine_cookie;

    xcb_get_property_cookie_t wm_icon_cookie;

    geomc = xcb_get_geometry(gConn, d);

    if ((attr = xcb_get_window_attributes_reply(gConn, cookie, 0)) == NULL) {
        DEBUG("Could not get attributes\n");
        xcb_discard_reply(gConn, geomc.sequence);
        return;
    }

    if (needsToBeMapped && attr->map_state != XCB_MAP_STATE_VIEWABLE) {
        xcb_discard_reply(gConn, geomc.sequence);
        goto out;
    }

    if (attr->override_redirect) {
        xcb_discard_reply(gConn, geomc.sequence);
        goto out;
    }

    if (container_by_window_id(window) != NULL) {
        DEBUG("already managed (by con %p)\n", container_by_window_id(window));
        xcb_discard_reply(gConn, geomc.sequence);
        goto out;
    }

    if ((geom = xcb_get_geometry_reply(gConn, geomc, 0)) == NULL) {
        DEBUG("could not get geometry\n");
        goto out;
    }

    uint32_t values[1];

    values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_void_cookie_t event_mask_cookie = xcb_change_window_attributes_checked(gConn, window, XCB_CW_EVENT_MASK, values);
    if (xcb_request_check(gConn, event_mask_cookie) != NULL) {
        DEBUG("Could not change event mask, the window probably already disappeared.\n");
        goto out;
    }

#define GET_PROPERTY(atom, len) xcb_get_property(gConn, false, window, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, len)

    wm_type_cookie = GET_PROPERTY(A__NET_WM_WINDOW_TYPE, UINT32_MAX);
    strut_cookie = GET_PROPERTY(A__NET_WM_STRUT_PARTIAL, UINT32_MAX);
    state_cookie = GET_PROPERTY(A__NET_WM_STATE, UINT32_MAX);
    utf8_title_cookie = GET_PROPERTY(A__NET_WM_NAME, 128);
    leader_cookie = GET_PROPERTY(A_WM_CLIENT_LEADER, UINT32_MAX);
    transient_cookie = GET_PROPERTY(XCB_ATOM_WM_TRANSIENT_FOR, UINT32_MAX);
    title_cookie = GET_PROPERTY(XCB_ATOM_WM_NAME, 128);
    class_cookie = GET_PROPERTY(XCB_ATOM_WM_CLASS, 128);
    role_cookie = GET_PROPERTY(A_WM_WINDOW_ROLE, 128);
    startup_id_cookie = GET_PROPERTY(A__NET_STARTUP_ID, 512);
    wm_hints_cookie = xcb_icccm_get_wm_hints(gConn, window);
    wm_normal_hints_cookie = xcb_icccm_get_wm_normal_hints(gConn, window);
    motif_wm_hints_cookie = GET_PROPERTY(A__MOTIF_WM_HINTS, 5 * sizeof(uint64_t));
    wm_user_time_cookie = GET_PROPERTY(A__NET_WM_USER_TIME, UINT32_MAX);
    wm_desktop_cookie = GET_PROPERTY(A__NET_WM_DESKTOP, UINT32_MAX);
    wm_machine_cookie = GET_PROPERTY(XCB_ATOM_WM_CLIENT_MACHINE, UINT32_MAX);
    wm_icon_cookie = GET_PROPERTY(A__NET_WM_ICON, UINT32_MAX);

    GWMWindow* cwindow = g_malloc0(sizeof(GWMWindow));
    EXIT_IF_MEM_IS_NULL(cwindow);
    cwindow->id = window;
    cwindow->depth = draw_util_get_visual_depth(attr->visual);

    int* buttons = key_binding_get_buttons_to_grab();
    xcb_gwm_grab_buttons(gConn, window, buttons);
    FREE(buttons);

    /* update as much information as possible so far (some replies may be NULL) */
    window_update_class(cwindow, xcb_get_property_reply(gConn, class_cookie, NULL));
    window_update_name_legacy(cwindow, xcb_get_property_reply(gConn, title_cookie, NULL));
    window_update_name(cwindow, xcb_get_property_reply(gConn, utf8_title_cookie, NULL));
    window_update_icon(cwindow, xcb_get_property_reply(gConn, wm_icon_cookie, NULL));
    window_update_leader(cwindow, xcb_get_property_reply(gConn, leader_cookie, NULL));
    window_update_transient_for(cwindow, xcb_get_property_reply(gConn, transient_cookie, NULL));
    window_update_strut_partial(cwindow, xcb_get_property_reply(gConn, strut_cookie, NULL));
    window_update_role(cwindow, xcb_get_property_reply(gConn, role_cookie, NULL));
    bool urgency_hint;
    window_update_hints(cwindow, xcb_get_property_reply(gConn, wm_hints_cookie, NULL), &urgency_hint);
    GWMBorderStyle motif_border_style;
    bool has_mwm_hints = window_update_motif_hints(cwindow, xcb_get_property_reply(gConn, motif_wm_hints_cookie, NULL), &motif_border_style);
    window_update_normal_hints(cwindow, xcb_get_property_reply(gConn, wm_normal_hints_cookie, NULL), geom);
    window_update_machine(cwindow, xcb_get_property_reply(gConn, wm_machine_cookie, NULL));
    xcb_get_property_reply_t *type_reply = xcb_get_property_reply(gConn, wm_type_cookie, NULL);
    xcb_get_property_reply_t *state_reply = xcb_get_property_reply(gConn, state_cookie, NULL);

    xcb_get_property_reply_t *startup_id_reply;
    startup_id_reply = xcb_get_property_reply(gConn, startup_id_cookie, NULL);
    char* startup_ws = startup_workspace_for_window(cwindow, startup_id_reply);
    DEBUG("startup workspace = %s\n", startup_ws);

    xcb_get_property_reply_t *wm_desktop_reply;
    wm_desktop_reply = xcb_get_property_reply(gConn, wm_desktop_cookie, NULL);
    cwindow->wmDesktop = NET_WM_DESKTOP_NONE;
    if (wm_desktop_reply != NULL && xcb_get_property_value_length(wm_desktop_reply) != 0) {
        uint32_t *wm_desktops = xcb_get_property_value(wm_desktop_reply);
        cwindow->wmDesktop = (int32_t)wm_desktops[0];
    }
    FREE(wm_desktop_reply);

    cwindow->needTakeFocus = window_supports_protocol(cwindow->id, A_WM_TAKE_FOCUS);
    cwindow->windowType = xcb_gwm_get_preferred_window_type(type_reply);

    GWMContainer* search_at = gContainerRoot;

    if (xcb_gwm_reply_contains_atom(type_reply, A__NET_WM_WINDOW_TYPE_DOCK)) {
        DEBUG("This window is of type dock");
        GWMOutput *output = randr_get_output_containing(geom->x, geom->y);
        if (output != NULL) {
            DEBUG("Starting search at output %s\n", output_primary_name(output));
            search_at = output->container;
        }

        if (cwindow->reserved.top > 0 && cwindow->reserved.bottom == 0) {
            DEBUG("Top dock client");
            cwindow->dock = W_DOCK_TOP;
        }
        else if (cwindow->reserved.top == 0 && cwindow->reserved.bottom > 0) {
            DEBUG("Bottom dock client");
            cwindow->dock = W_DOCK_BOTTOM;
        }
        else {
            DEBUG("Ignoring invalid reserved edges (_NET_WM_STRUT_PARTIAL), using position as fallback:\n");
            if (geom->y < (int16_t)(search_at->rect.height / 2)) {
                DEBUG("geom->y = %d < rect.height / 2 = %d, it is a top dock client", geom->y, (search_at->rect.height / 2));
                cwindow->dock = W_DOCK_TOP;
            } else {
                DEBUG("geom->y = %d >= rect.height / 2 = %d, it is a bottom dock client", geom->y, (search_at->rect.height / 2));
                cwindow->dock = W_DOCK_BOTTOM;
            }
        }
    }

    DEBUG("Initial geometry: (%d, %d, %d, %d)\n", geom->x, geom->y, geom->width, geom->height);

    /* See if any container swallows this new window */
    cwindow->swallowed = false;
    GWMMatch *match = NULL;
    GWMContainer* nc = container_for_window(search_at, cwindow, &match);
    const bool match_from_restart_mode = (match && match->restartMode);
    if (nc == NULL) {
        GWMContainer* wm_desktop_ws = NULL;
        GWMAssignment *assignment;

        /* If not, check if it is assigned to a specific workspace */
        if ((assignment = assignment_for(cwindow, A_TO_WORKSPACE)) ||
            (assignment = assignment_for(cwindow, A_TO_WORKSPACE_NUMBER))) {
            DEBUG("Assignment matches (%p)\n", match);

            GWMContainer* assigned_ws = NULL;
            if (assignment->type == A_TO_WORKSPACE_NUMBER) {
                long parsed_num = util_ws_name_to_number(assignment->destination.workspace);

                assigned_ws = workspace_get_existing_workspace_by_num(parsed_num);
            }
            if (!assigned_ws) {
                assigned_ws = workspace_get(assignment->destination.workspace);
            }

            nc = container_descend_tiling_focused(assigned_ws);
            DEBUG("focused on ws %s: %p / %s\n", assigned_ws->name, nc, nc->name);
            if (nc->type == CT_WORKSPACE) {
                nc = tree_open_container(nc, cwindow);
            }
            else {
                nc = tree_open_container(nc->parent, cwindow);
            }

            if (!workspace_is_visible(assigned_ws)) {
                urgency_hint = true;
            }
        }
        else if (cwindow->wmDesktop != NET_WM_DESKTOP_NONE
            && cwindow->wmDesktop != NET_WM_DESKTOP_ALL
            && (wm_desktop_ws = extend_wm_hint_get_workspace_by_index(cwindow->wmDesktop)) != NULL) {
            DEBUG("Using workspace %p / %s because _NET_WM_DESKTOP = %d.", wm_desktop_ws, wm_desktop_ws->name, cwindow->wmDesktop);

            nc = container_descend_tiling_focused(wm_desktop_ws);
            if (nc->type == CT_WORKSPACE) {
                nc = tree_open_container(nc, cwindow);
            }
            else {
                nc = tree_open_container(nc->parent, cwindow);
            }
        }
        else if (startup_ws) {
            DEBUG("Using workspace on which this application was started (%s)\n", startup_ws);
            nc = container_descend_tiling_focused(workspace_get(startup_ws));
            DEBUG("focused on ws %s: %p / %s\n", startup_ws, nc, nc->name);
            if (nc->type == CT_WORKSPACE) {
                nc = tree_open_container(nc, cwindow);
            }
            else {
                nc = tree_open_container(nc->parent, cwindow);
            }
        }
        else {
            if (gFocused->type == CT_CON && container_accepts_window(gFocused)) {
                DEBUG("using current container, focused = %p, focused->name = %s", gFocused, gFocused->name);
                nc = gFocused;
            }
            else {
                nc = tree_open_container(NULL, cwindow);
            }
        }

        if ((assignment = assignment_for(cwindow, A_TO_OUTPUT))) {
            container_move_to_output_name(nc, assignment->destination.output, true);
        }
    }
    else {
        if (match != NULL && match->insertWhere == M_BELOW) {
            nc = tree_open_container(nc, cwindow);
        }

        if (match != NULL && match->insertWhere != M_BELOW) {
            DEBUG("Removing match %p from container %p", match, nc);
            TAILQ_REMOVE(&(nc->swallowHead), match, matches);
            match_free(match);
            FREE(match);
        }
        cwindow->swallowed = true;
    }

    DEBUG("new container = %p", nc);
    if (nc->window != NULL && nc->window != cwindow) {
        if (!restore_kill_placeholder(nc->window->id)) {
            DEBUG("Uh?! Container without a placeholder, but with a window, has swallowed this to-be-managed window?!\n");
        } else {
            _remove_matches(nc);
        }
    }
    xcb_window_t old_frame = XCB_NONE;
    if (nc->window != cwindow && nc->window != NULL) {
        window_free(nc->window);
        old_frame = _match_depth(cwindow, nc);
    }
    nc->window = cwindow;
    x_reinit(nc);

    nc->borderWidth = geom->border_width;

    char *name = g_strdup_printf("[gwm con] container around %p", cwindow);
    x_set_name(nc, name);
    free(name);

    GWMContainer* ws = container_get_workspace(nc);
    GWMContainer* fs = container_get_full_screen_covering_ws(ws);

    if (xcb_gwm_reply_contains_atom(state_reply, A__NET_WM_STATE_FULLSCREEN)) {
        if (fs != nc) {
            GWMOutput *output = randr_get_output_with_dimensions((GWMRect){geom->x, geom->y, geom->width, geom->height});
            if (output != NULL) {
                container_move_to_output(nc, output, false);
            }
            container_toggle_full_screen(nc, CF_OUTPUT);
        }
        fs = NULL;
    }

    bool set_focus = false;

    if (fs == NULL) {
        DEBUG("Not in fullscreen mode, focusing\n");
        if (!cwindow->dock) {
            GWMContainer* current_output = container_get_output(gFocused);
            GWMContainer* target_output = container_get_output(ws);
            if (workspace_is_visible(ws) && current_output == target_output) {
                if (!match_from_restart_mode) {
                    set_focus = true;
                }
                else {
                    DEBUG("not focusing, matched with restart_mode == true\n");
                }
            }
            else {
                DEBUG("workspace not visible, not focusing\n");
            }
        }
        else {
            DEBUG("dock, not focusing\n");
        }
    }
    else {
        DEBUG("fs = %p, ws = %p, not focusing\n", fs, ws);
        GWMContainer* first = TAILQ_FIRST(&(nc->parent->focusHead));
        if (first != nc) {
            TAILQ_REMOVE(&(nc->parent->focusHead), nc, focused);
            TAILQ_INSERT_AFTER(&(nc->parent->focusHead), first, nc, focused);
        }
    }

    bool want_floating = false;
    if (xcb_gwm_reply_contains_atom(type_reply, A__NET_WM_WINDOW_TYPE_DIALOG)
        || xcb_gwm_reply_contains_atom(type_reply, A__NET_WM_WINDOW_TYPE_UTILITY)
        || xcb_gwm_reply_contains_atom(type_reply, A__NET_WM_WINDOW_TYPE_TOOLBAR)
        || xcb_gwm_reply_contains_atom(type_reply, A__NET_WM_WINDOW_TYPE_SPLASH)
        || xcb_gwm_reply_contains_atom(state_reply, A__NET_WM_STATE_MODAL)
        || (cwindow->maxWidth > 0
            && cwindow->maxHeight > 0
            && cwindow->minHeight == cwindow->maxHeight
            && cwindow->minWidth == cwindow->maxWidth)) {
        DEBUG("This window is a dialog window, setting floating\n");
        want_floating = true;
    }

    if (xcb_gwm_reply_contains_atom(state_reply, A__NET_WM_STATE_STICKY)) {
        nc->sticky = true;
    }
    if (cwindow->wmDesktop == NET_WM_DESKTOP_ALL && (ws == NULL || !container_is_internal(ws))) {
        DEBUG("This window has _NET_WM_DESKTOP = 0xFFFFFFFF. Will float it and make it sticky.");
        nc->sticky = true;
        want_floating = true;
    }

    FREE(state_reply);
    FREE(type_reply);

    if (cwindow->transientFor != XCB_NONE
        || (cwindow->leader != XCB_NONE
            && cwindow->leader != cwindow->id
            && container_by_window_id(cwindow->leader) != NULL)) {
        DEBUG("This window is transient for another window, setting floating");
        want_floating = true;

        if (gConfig.popupDuringFullscreen == PDF_LEAVE_FULLSCREEN && fs != NULL) {
            DEBUG("There is a fullscreen window, leaving fullscreen mode\n");
            container_toggle_full_screen(fs, CF_OUTPUT);
        }
        else if (gConfig.popupDuringFullscreen == PDF_SMART && fs != NULL && fs->window != NULL) {
            set_focus = container_find_transient_for_window(nc, fs->window->id);
        }
    }

    if (cwindow->dock) {
        want_floating = false;
    }

    if (nc->geoRect.width == 0) {
        nc->geoRect = (GWMRect){geom->x, geom->y, geom->width, geom->height};
    }

    if (want_floating) {
        DEBUG("geometry = %d x %d\n", nc->geoRect.width, nc->geoRect.height);
        if (floating_enable(nc, true)) {
            nc->floating = FLOATING_AUTO_ON;
        }
    }

    if (has_mwm_hints) {
        DEBUG("MOTIF_WM_HINTS specifies decorations (border_style = %d)\n", motif_border_style);
        if (want_floating) {
            container_set_border_style(nc, motif_border_style, gConfig.defaultFloatingBorderWidth);
        }
        else {
            container_set_border_style(nc, motif_border_style, gConfig.defaultBorderWidth);
        }
    }

    if (nc->currentBorderWidth == -1) {
        nc->currentBorderWidth = (want_floating ? gConfig.defaultFloatingBorderWidth : gConfig.defaultBorderWidth);
    }

    values[0] = XCB_NONE;
    xcb_change_window_attributes(gConn, window, XCB_CW_EVENT_MASK, values);

    xcb_void_cookie_t rcookie = xcb_reparent_window_checked(gConn, window, nc->frame.id, 0, 0);
    if (xcb_request_check(gConn, rcookie) != NULL) {
        DEBUG("Could not reparent the window, aborting\n");
        goto geom_out;
    }

    values[0] = CHILD_EVENT_MASK & ~XCB_EVENT_MASK_ENTER_WINDOW;
    xcb_change_window_attributes(gConn, window, XCB_CW_EVENT_MASK, values);
    xcb_flush(gConn);
    xcb_change_save_set(gConn, XCB_SET_MODE_INSERT, window);

    if (gShapeSupported) {
        xcb_shape_select_input(gConn, window, true);
        xcb_shape_query_extents_cookie_t cookie = xcb_shape_query_extents(gConn, window);
        xcb_shape_query_extents_reply_t *reply = xcb_shape_query_extents_reply(gConn, cookie, NULL);
        if (reply != NULL && reply->bounding_shaped) {
            cwindow->shaped = true;
        }
        FREE(reply);
    }

    assignments_run(cwindow);
    ws = container_get_workspace(nc);
    if (ws && !workspace_is_visible(ws)) {
        ws->rect = ws->parent->rect;
        render_container(ws);
        set_focus = false;
    }
    render_container(gContainerRoot);
    cwindow->managedSince = time(NULL);

//    ipc_send_window_event("new", nc);

    if (set_focus && assignment_for(cwindow, A_NO_FOCUS) != NULL) {
        if (container_num_windows(ws) == 1) {
            DEBUG("This is the first window on this workspace, ignoring no_focus.\n");
        }
        else {
            DEBUG("no_focus was set for con = %p, not setting focus.\n", nc);
            set_focus = false;
        }
    }

    if (set_focus) {
        DEBUG("Checking con = %p for _NET_WM_USER_TIME.\n", nc);

        uint32_t *wm_user_time;
        xcb_get_property_reply_t *wm_user_time_reply = xcb_get_property_reply(gConn, wm_user_time_cookie, NULL);
        if (wm_user_time_reply != NULL && xcb_get_property_value_length(wm_user_time_reply) != 0 &&
            (wm_user_time = xcb_get_property_value(wm_user_time_reply)) &&
            wm_user_time[0] == 0) {
            DEBUG("_NET_WM_USER_TIME set to 0, not focusing con = %p.\n", nc);
            set_focus = false;
        }

        FREE(wm_user_time_reply);
    }
    else {
        xcb_discard_reply(gConn, wm_user_time_cookie.sequence);
    }

    if (set_focus) {
        if (nc->window->doesNotAcceptFocus && !nc->window->needTakeFocus) {
            set_focus = false;
        }
    }

    if (set_focus && nc->mapped) {
        DEBUG("Now setting focus.");
        container_activate(nc);
    }

    tree_render();

    if (old_frame != XCB_NONE) {
        xcb_destroy_window(gConn, old_frame);
    }

    container_set_urgency(nc, urgency_hint);

    cwindow->wmDesktop = NET_WM_DESKTOP_NONE;
    extend_wm_hint_update_wm_desktop();

    output_push_sticky_windows(gFocused);

geom_out:
    free(geom);
out:
    free(attr);
}


static xcb_window_t _match_depth(GWMWindow* win, GWMContainer* con)
{
    xcb_window_t old_frame = XCB_NONE;
    if (con->depth != win->depth) {
        old_frame = con->frame.id;
        con->depth = win->depth;
        x_container_reframe(con);
    }
    return old_frame;
}


static void _remove_matches(GWMContainer* con)
{
    while (!TAILQ_EMPTY(&(con->swallowHead))) {
        GWMMatch *first = TAILQ_FIRST(&(con->swallowHead));
        TAILQ_REMOVE(&(con->swallowHead), first, matches);
        match_free(first);
        free(first);
    }
}