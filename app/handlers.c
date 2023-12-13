//
// Created by dingjing on 23-11-24.
//

#include "handlers.h"

#include <glib.h>
#include <time.h>
#include <xcb/xkb.h>
#include <sys/time.h>
#include <xcb/randr.h>
#include <xcb/xcb_keysyms.h>
#include <libsn/sn-monitor.h>

#include "x.h"
#include "val.h"
#include "log.h"
#include "xcb.h"
#include "tree.h"
#include "sync.h"
#include "drag.h"
#include "randr.h"
#include "utils.h"
#include "resize.h"
#include "render.h"
#include "config.h"
#include "manage.h"
#include "window.h"
#include "output.h"
#include "startup.h"
#include "floating.h"
#include "container.h"
#include "draw-util.h"
#include "workspace.h"
#include "scratchpad.h"
#include "key-bindings.h"
#include "extend-wm-hints.h"


#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT     0
#define _NET_WM_MOVERESIZE_SIZE_TOP         1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT    2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT       3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM      5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT  6
#define _NET_WM_MOVERESIZE_SIZE_LEFT        7
#define _NET_WM_MOVERESIZE_MOVE             8       /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD    9       /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10      /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL           11      /* cancel operation */

#define _NET_MOVERESIZE_WINDOW_X            (1 << 8)
#define _NET_MOVERESIZE_WINDOW_Y            (1 << 9)
#define _NET_MOVERESIZE_WINDOW_WIDTH        (1 << 10)
#define _NET_MOVERESIZE_WINDOW_HEIGHT       (1 << 11)


typedef enum {
    CLICK_BORDER = 0,
    CLICK_DECORATION = 1,
    CLICK_INSIDE = 2
} click_destination_t;


typedef struct GWMPropertyHandler           GWMPropertyHandler;
typedef bool (*GWMPropertyHandlerCB) (GWMContainer* con, xcb_get_property_reply_t* property);

// handler_handle start
static void handle_focus_in(xcb_focus_in_event_t *event);
static void handle_focus_out(xcb_focus_in_event_t *event);
static void handle_key_press(xcb_key_press_event_t *event);
static void handle_map_request(xcb_map_request_event_t *event);
static void handle_button_press(xcb_button_press_event_t *event);
static void handle_enter_notify(xcb_enter_notify_event_t *event);
static void handle_motion_notify(xcb_motion_notify_event_t *event);
static void handle_mapping_notify(xcb_mapping_notify_event_t *event);
static void handle_client_message(xcb_client_message_event_t *event);
static void handle_unmap_notify_event(xcb_unmap_notify_event_t *event);
static void handle_selection_clear(xcb_selection_clear_event_t *event);
static void handle_configure_notify(xcb_configure_notify_event_t *event);
static void handle_destroy_notify_event(xcb_destroy_notify_event_t *event);
static void handle_configure_request(xcb_configure_request_event_t *event);

//
static void allow_replay_pointer(xcb_timestamp_t time);
static void handle_screen_change(xcb_generic_event_t *e);
static void check_crossing_screen_boundary(uint32_t x, uint32_t y);
static void property_notify(uint8_t state, xcb_window_t window, xcb_atom_t atom);
static bool floating_mod_on_tiled_client(GWMContainer* con, xcb_button_press_event_t *event);
static void route_click(GWMContainer* con, xcb_button_press_event_t *event, bool modPressed, click_destination_t dest);
static bool tiling_resize(GWMContainer* con, xcb_button_press_event_t *event, click_destination_t dest, bool useThreshold);
static bool tiling_resize_for_border(GWMContainer* con, GWMBorder border, xcb_button_press_event_t *event, bool useThreshold);
// handler_handle end

static void handler_handle_expose_event(xcb_expose_event_t *event);
static bool handler_handle_hints(GWMContainer* con, xcb_get_property_reply_t *reply);
static bool handler_handle_gwm_floating(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_class_change(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_window_type(GWMContainer* con, xcb_get_property_reply_t *reply);
static bool handler_handle_normal_hints(GWMContainer* con, xcb_get_property_reply_t *reply);
static bool handler_handle_transient_for(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_machine_change(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_window_icontainer_change(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_window_name_change(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_window_role_change(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_motif_hints_change(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_client_leader_change(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_strut_partial_change(GWMContainer* con, xcb_get_property_reply_t *prop);
static bool handler_handle_window_name_change_legacy(GWMContainer* con, xcb_get_property_reply_t *prop);


struct GWMPropertyHandler
{
    xcb_atom_t                  atom;
    uint32_t                    longLen;
    GWMPropertyHandlerCB        cb;
};



static SLIST_HEAD(IgnoreHead, IgnoreEvent)  gsIgnoreEvents;
static GWMPropertyHandler gPropertyHandlers[] =
    {
        {0, 128, handler_handle_window_name_change},
        {0, UINT_MAX, handler_handle_hints},
        {0, 128, handler_handle_window_name_change_legacy},
        {0, UINT_MAX, handler_handle_normal_hints},
        {0, UINT_MAX, handler_handle_client_leader_change},
        {0, UINT_MAX, handler_handle_transient_for},
        {0, 128, handler_handle_window_role_change},
        {0, 128, handler_handle_class_change},
        {0, UINT_MAX, handler_handle_strut_partial_change},
        {0, UINT_MAX, handler_handle_window_type},
        {0, UINT_MAX, handler_handle_gwm_floating},
        {0, 128, handler_handle_machine_change},
        {0, 5 * sizeof(uint16_t), handler_handle_motif_hints_change},
        {0, UINT_MAX, handler_handle_window_icontainer_change},
    };
#define HANDLERS_NUM (sizeof (gPropertyHandlers) / sizeof (GWMPropertyHandler))


void handler_property_init()
{
    sn_monitor_context_new (gSnDisplay, gConnScreen, startup_monitor_event, NULL, NULL);

    gPropertyHandlers[0].atom = A__NET_WM_NAME;
    gPropertyHandlers[1].atom = XCB_ATOM_WM_HINTS;
    gPropertyHandlers[2].atom = XCB_ATOM_WM_NAME;
    gPropertyHandlers[3].atom = XCB_ATOM_WM_NORMAL_HINTS;
    gPropertyHandlers[4].atom = A_WM_CLIENT_LEADER;
    gPropertyHandlers[5].atom = XCB_ATOM_WM_TRANSIENT_FOR;
    gPropertyHandlers[6].atom = A_WM_WINDOW_ROLE;
    gPropertyHandlers[7].atom = XCB_ATOM_WM_CLASS;
    gPropertyHandlers[8].atom = A__NET_WM_STRUT_PARTIAL;
    gPropertyHandlers[9].atom = A__NET_WM_WINDOW_TYPE;
    gPropertyHandlers[10].atom = A_GWM_FLOATING_WINDOW;
    gPropertyHandlers[11].atom = XCB_ATOM_WM_CLIENT_MACHINE;
    gPropertyHandlers[12].atom = A__MOTIF_WM_HINTS;
    gPropertyHandlers[13].atom = A__NET_WM_ICON;
}

void handler_add_ignore_event(int sequence, int responseType)
{
    GWMIgnoreEvent* event = g_malloc0(sizeof(GWMIgnoreEvent));
    EXIT_IF_MEM_IS_NULL(event);

    event->sequence = sequence;
    event->responseType = responseType;
    event->added = time(NULL);

    SLIST_INSERT_HEAD(&gsIgnoreEvents, event, ignoreEvents);
}

bool handler_event_is_ignored(int sequence, int responseType)
{
    GWMIgnoreEvent* event;
    time_t now = time(NULL);
    for (event = SLIST_FIRST(&gsIgnoreEvents); event != SLIST_END(&gsIgnoreEvents);) {
        if ((now - event->added) > 5) {
            GWMIgnoreEvent* save = event;
            event = SLIST_NEXT(event, ignoreEvents);
            SLIST_REMOVE(&gsIgnoreEvents, save, IgnoreEvent, ignoreEvents);
            FREE(save);
        }
        else {
            event = SLIST_NEXT(event, ignoreEvents);
        }
    }

    SLIST_FOREACH (event, &gsIgnoreEvents, ignoreEvents) {
        if (event->sequence != sequence) {
            continue;
        }

        if (event->responseType != -1 && event->responseType != responseType) {
            continue;
        }
        return true;
    }

    return false;
}

void handler_handle_event(int type, xcb_generic_event_t *event)
{
//    DEBUG("handle event: %d", type);
    if (type != XCB_MOTION_NOTIFY) {
        DEBUG("event type %d, xkb_base %d", type, gXKBBase);
    }

    if (gRandrBase > -1 && type == gRandrBase + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
        handle_screen_change(event);
        return;
    }

    if (gXKBBase > -1 && type == gXKBBase) {
        DEBUG("xkb event, need to handle it.");
        xcb_xkb_state_notify_event_t *state = (xcb_xkb_state_notify_event_t *)event;
        if (state->xkbType == XCB_XKB_NEW_KEYBOARD_NOTIFY) {
            DEBUG("xkb new keyboard notify, sequence %d, time %d", state->sequence, state->time);
            xcb_key_symbols_free(gKeySyms);
            gKeySyms = xcb_key_symbols_alloc(gConn);
            if (((xcb_xkb_new_keyboard_notify_event_t *)event)->changed & XCB_XKB_NKN_DETAIL_KEYCODES) {
                (void)key_binding_load_keymap();
            }
            config_ungrab_all_keys(gConn);
            key_binding_translate_keysyms();
            key_binding_grab_all_keys(gConn);
        }
        else if (state->xkbType == XCB_XKB_MAP_NOTIFY) {
            if (handler_event_is_ignored(event->sequence, type)) {
                DEBUG("Ignoring map notify event for sequence %d.", state->sequence);
            }
            else {
                DEBUG("xkb map notify, sequence %d, time %d", state->sequence, state->time);
                handler_add_ignore_event(event->sequence, type);
                xcb_key_symbols_free(gKeySyms);
                gKeySyms = xcb_key_symbols_alloc(gConn);
                config_ungrab_all_keys(gConn);
                key_binding_translate_keysyms();
                key_binding_grab_all_keys(gConn);
                (void)key_binding_load_keymap();
            }
        }
        else if (state->xkbType == XCB_XKB_STATE_NOTIFY) {
            DEBUG("xkb state group = %d", state->group);
            if (gXKBCurrentGroup == state->group) {
                return;
            }
            gXKBCurrentGroup = state->group;
            key_binding_ungrab_all_keys(gConn);
            key_binding_grab_all_keys(gConn);
        }
        return;
    }

    if (gShapeSupported && type == gShapeBase + XCB_SHAPE_NOTIFY) {
        xcb_shape_notify_event_t *shape = (xcb_shape_notify_event_t *)event;

        DEBUG("shape_notify_event for window 0x%08x, shape_kind = %d, shaped = %d",
                shape->affected_window, shape->shape_kind, shape->shaped);

        GWMContainer* con = container_by_window_id(shape->affected_window);
        if (con == NULL) {
            DEBUG("Not a managed window 0x%08x, ignoring shape_notify_event", shape->affected_window);
            return;
        }

        if (shape->shape_kind == XCB_SHAPE_SK_BOUNDING || shape->shape_kind == XCB_SHAPE_SK_INPUT) {
            x_set_shape(con, shape->shape_kind, shape->shaped);
        }

        return;
    }

    switch (type) {
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
            handle_key_press((xcb_key_press_event_t *)event);
            break;

        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE:
            handle_button_press((xcb_button_press_event_t *)event);
            break;

        case XCB_MAP_REQUEST:
            handle_map_request((xcb_map_request_event_t *)event);
            break;

        case XCB_UNMAP_NOTIFY:
            handle_unmap_notify_event((xcb_unmap_notify_event_t *)event);
            break;

        case XCB_DESTROY_NOTIFY:
            handle_destroy_notify_event((xcb_destroy_notify_event_t *)event);
            break;

        case XCB_EXPOSE:
            if (((xcb_expose_event_t *)event)->count == 0) {
                handler_handle_expose_event((xcb_expose_event_t *)event);
            }

            break;

        case XCB_MOTION_NOTIFY:
            handle_motion_notify((xcb_motion_notify_event_t *)event);
            break;

            /* Enter window = user moved their mouse over the window */
        case XCB_ENTER_NOTIFY:
            handle_enter_notify((xcb_enter_notify_event_t *)event);
            break;

            /* Client message are sent to the root window. The only interesting
             * client message for us is _NET_WM_STATE, we honour
             * _NET_WM_STATE_FULLSCREEN and _NET_WM_STATE_DEMANDS_ATTENTION */
        case XCB_CLIENT_MESSAGE:
            handle_client_message((xcb_client_message_event_t *)event);
            break;

            /* Configure request = window tried to change size on its own */
        case XCB_CONFIGURE_REQUEST:
            handle_configure_request((xcb_configure_request_event_t *)event);
            break;

            /* Mapping notify = keyboard mapping changed (Xmodmap), re-grab bindings */
        case XCB_MAPPING_NOTIFY:
            handle_mapping_notify((xcb_mapping_notify_event_t *)event);
            break;

        case XCB_FOCUS_IN:
            handle_focus_in((xcb_focus_in_event_t *)event);
            break;

        case XCB_FOCUS_OUT:
            handle_focus_out((xcb_focus_out_event_t *)event);
            break;

        case XCB_PROPERTY_NOTIFY: {
            xcb_property_notify_event_t *e = (xcb_property_notify_event_t *)event;
            gLastTimestamp = e->time;
            property_notify(e->state, e->window, e->atom);
            break;
        }

        case XCB_CONFIGURE_NOTIFY:
            handle_configure_notify((xcb_configure_notify_event_t *)event);
            break;

        case XCB_SELECTION_CLEAR:
            handle_selection_clear((xcb_selection_clear_event_t *)event);
            break;

        default:
            /* DEBUG("Unhandled event of type %d", type); */
            break;
    }
}

static bool handler_handle_window_name_change(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    g_autofree char *oldName = (con->window->name != NULL ? g_strdup(con->window->name) : NULL);

    window_update_name(con->window, prop);

    con = manage_remanage_window(con);

    x_push_changes(gContainerRoot);

//    if (window_name_changed(con->window, oldName)) {
//        ipc_send_window_event("title", con);
//    }

    return true;
}

static bool handler_handle_window_name_change_legacy(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    g_autofree char *oldName = (con->window->name != NULL ? g_strdup(con->window->name) : NULL);

    window_update_name_legacy(con->window, prop);

    con = manage_remanage_window(con);

    x_push_changes(gContainerRoot);

//    if (window_name_changed(con->window, old_name))
//        ipc_send_window_event("title", con);
//

    return true;
}

static bool handler_handle_window_role_change(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    window_update_role(con->window, prop);

    con = manage_remanage_window(con);

    return true;
}

static void handler_handle_expose_event(xcb_expose_event_t *event)
{
    GWMContainer* parent;

    DEBUG("window = %08x", event->window);

    if ((parent = container_by_frame_id(event->window)) == NULL) {
        INFO("expose event for unknown window, ignoring");
        return;
    }

    /* Since we render to our surface on every change anyways, expose events
     * only tell us that the X server lost (parts of) the window contents. */
    draw_util_copy_surface(&(parent->frameBuffer), &(parent->frame), 0, 0, 0, 0, parent->rect.width, parent->rect.height);
    xcb_flush(gConn);
}

static bool handler_handle_hints(GWMContainer* con, xcb_get_property_reply_t *reply)
{
    bool urgencyHint;
    window_update_hints(con->window, reply, &urgencyHint);
    container_set_urgency(con, urgencyHint);
    manage_remanage_window(con);
    tree_render();

    return true;
}

static bool handler_handle_transient_for(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    window_update_transient_for(con->window, prop);
    return true;
}

static bool handler_handle_client_leader_change(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    window_update_leader(con->window, prop);
    return true;
}

static bool handler_handle_normal_hints(GWMContainer* con, xcb_get_property_reply_t *reply)
{
    bool changed = window_update_normal_hints(con->window, reply, NULL);

    if (changed) {
        GWMContainer* floating = container_inside_floating(con);
        if (floating) {
            floating_check_size(con, false);
            tree_render();
        }
    }

    {
        FREE(reply);
        reply = NULL;
    }

    return true;
}

static bool handler_handle_window_type(GWMContainer* con, xcb_get_property_reply_t *reply)
{
    window_update_type(con->window, reply);
    return true;
}

static bool handler_handle_class_change(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    window_update_class(con->window, prop);
    con = manage_remanage_window(con);

    return true;
}

static bool handler_handle_machine_change(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    window_update_machine(con->window, prop);
    con = manage_remanage_window(con);

    return true;
}

static bool handler_handle_motif_hints_change(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    GWMBorderStyle motif_border_style;
    bool has_mwm_hints = window_update_motif_hints(con->window, prop, &motif_border_style);

    if (has_mwm_hints && motif_border_style != con->borderStyle) {
        DEBUG("Update border style of con %p to %d", con, motif_border_style);
        container_set_border_style(con, motif_border_style, con->currentBorderWidth);

        x_push_changes(gContainerRoot);
    }

    return true;
}

static bool handler_handle_strut_partial_change(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    window_update_strut_partial(con->window, prop);

    /* we only handle this change for dock clients */
    if (con->parent == NULL || con->parent->type != CT_DOCK_AREA) {
        return true;
    }

    GWMContainer* searchAt = gContainerRoot;
    GWMContainer* output = container_get_output(con);
    if (output != NULL) {
        DEBUG("Starting search at output %s", output->name);
        searchAt = output;
    }

    /* find out the desired position of this dock window */
    if (con->window->reserved.top > 0 && con->window->reserved.bottom == 0) {
        DEBUG("Top dock client");
        con->window->dock = W_DOCK_TOP;
    }
    else if (con->window->reserved.top == 0 && con->window->reserved.bottom > 0) {
        DEBUG("Bottom dock client");
        con->window->dock = W_DOCK_BOTTOM;
    }
    else {
        DEBUG("Ignoring invalid reserved edges (_NET_WM_STRUT_PARTIAL), using position as fallback:");
        if (con->geoRect.y < (searchAt->rect.height / 2)) {
            DEBUG("geom->y = %d < rect.height / 2 = %d, it is a top dock client", con->geoRect.y, (searchAt->rect.height / 2));
            con->window->dock = W_DOCK_TOP;
        }
        else {
            DEBUG("geom->y = %d >= rect.height / 2 = %d, it is a bottom dock client", con->geoRect.y, (searchAt->rect.height / 2));
            con->window->dock = W_DOCK_BOTTOM;
        }
    }

    GWMContainer* dockArea = container_for_window(searchAt, con->window, NULL);
    g_return_val_if_fail(dockArea, false);

    /* attach the dock to the dock area */
    container_detach(con);
    container_attach(con, dockArea, true);

    tree_render();

    return true;
}

static bool handler_handle_gwm_floating(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    DEBUG("floating change for con %p", con);

    manage_remanage_window(con);

    return true;
}

static bool handler_handle_window_icontainer_change(GWMContainer* con, xcb_get_property_reply_t *prop)
{
    window_update_icon(con->window, prop);

    x_push_changes(gContainerRoot);

    return true;
}

static void handle_key_press(xcb_key_press_event_t *event)
{
    const bool key_release = (event->response_type == XCB_KEY_RELEASE);

    gLastTimestamp = event->time;

    DEBUG("%s %d, state raw = 0x%x", (key_release ? "KeyRelease" : "KeyPress"), event->detail, event->state);

    GWMBinding *bind = key_binding_get_binding_from_xcb_event((xcb_generic_event_t *)event);
    if (bind == NULL) {
        return;
    }

//    CommandResult *result = run_binding(bind, NULL);
//    command_result_free(result);
}

static void handle_button_press(xcb_button_press_event_t *event)
{
    GWMContainer* con;
    DEBUG("Button %d (state %d) %s on window 0x%08x (child 0x%08x) at (%d, %d) (root %d, %d)",
            event->detail, event->state, (event->response_type == XCB_BUTTON_PRESS ? "press" : "release"),
            event->event, event->child, event->event_x, event->event_y, event->root_x,
            event->root_y);

    gLastTimestamp = event->time;

    const uint32_t mod = (gConfig.floatingModifier & 0xFFFF);
    const bool mod_pressed = (mod != 0 && (event->state & mod) == mod);
    DEBUG("floating_mod = %d, detail = %d", mod_pressed, event->detail);
    if ((con = container_by_window_id(event->event))) {
        route_click(con, event, mod_pressed, CLICK_INSIDE);
        return;
    }

    if (!(con = container_by_frame_id(event->event))) {
        if (event->event == gRoot) {
            GWMBinding *bind = key_binding_get_binding_from_xcb_event((xcb_generic_event_t *)event);
            if (bind != NULL && bind->wholeWindow) {
//                CommandResult *result = run_binding(bind, NULL);
//                command_result_free(result);
            }
        }

        if (event->event == gRoot && event->response_type == XCB_BUTTON_PRESS) {
            GWMContainer* output, *ws;
            TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
                if (container_is_internal(output)
                    || !util_rect_contains(output->rect, event->event_x, event->event_y)) {
                    continue;
                }

                ws = TAILQ_FIRST(&(output_get_content(output)->focusHead));
                if (ws != container_get_workspace(gFocused)) {
                    workspace_show(ws);
                    tree_render();
                }
                return;
            }
            return;
        }
        ERROR("Clicked into unknown window?!");
        xcb_allow_events(gConn, XCB_ALLOW_REPLAY_POINTER, event->time);
        xcb_flush(gConn);
        return;
    }

    if (con->window != NULL) {
        if (util_rect_contains(con->decorationRect, event->event_x, event->event_y)) {
            route_click(con, event, mod_pressed, CLICK_DECORATION);
            return;
        }
    }
    else {
        GWMContainer* child;
        TAILQ_FOREACH_REVERSE (child, &(con->nodesHead), nodesHead, nodes) {
            if (!util_rect_contains(child->decorationRect, event->event_x, event->event_y)) {
                continue;
            }

            route_click(child, event, mod_pressed, CLICK_DECORATION);
            return;
        }
    }

    if (event->child != XCB_NONE) {
        DEBUG("event->child not XCB_NONE, so this is an event which originated from a click into the application, but the application did not handle it.");
        route_click(con, event, mod_pressed, CLICK_INSIDE);
        return;
    }

    route_click(con, event, mod_pressed, CLICK_BORDER);
}

static void route_click(GWMContainer* con, xcb_button_press_event_t *event, bool mod_pressed, click_destination_t dest)
{
    DEBUG("--> click properties: mod = %d, destination = %d", mod_pressed, dest);
    DEBUG("--> OUTCOME = %p", con);
    DEBUG("type = %d, name = %s", con->type, con->name);

    /* don’t handle dock area cons, they must not be focused */
    if (con->parent->type == CT_DOCK_AREA) {
        allow_replay_pointer(event->time);
        return;
    }

    GWMBinding *bind = key_binding_get_binding_from_xcb_event((xcb_generic_event_t*)event);
    if (bind && ((dest == CLICK_DECORATION && !bind->excludeTitleBar)
        || (dest == CLICK_INSIDE && bind->wholeWindow)
        || (dest == CLICK_BORDER && bind->border))) {
//        CommandResult *result = run_binding(bind, con);

        /* ASYNC_POINTER eats the event */
        xcb_allow_events(gConn, XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(gConn);

//        command_result_free(result);
        return;
    }

    /* There is no default behavior for button release events so we are done. */
    if (event->response_type == XCB_BUTTON_RELEASE) {
        allow_replay_pointer(event->time);
        return;
    }

    GWMContainer* ws = container_get_workspace(con);
    GWMContainer* focused_workspace = container_get_workspace(gFocused);

    if (!ws) {
        ws = TAILQ_FIRST(&(output_get_content(container_get_output(con))->focusHead));
        if (!ws) {
            allow_replay_pointer(event->time);
            return;
        }
    }

    /* get the floating con */
    GWMContainer* floatingcon = container_inside_floating(con);
    const bool proportional = (event->state & XCB_KEY_BUT_MASK_SHIFT) == XCB_KEY_BUT_MASK_SHIFT;
    const bool in_stacked = (con->parent->layout == L_STACKED || con->parent->layout == L_TABBED);
    const bool was_focused = gFocused == con;
    const bool is_left_click = (event->detail == XCB_BUTTON_CLICK_LEFT);
    const bool is_right_click = (event->detail == XCB_BUTTON_CLICK_RIGHT);
    const bool is_left_or_right_click = (is_left_click || is_right_click);
    const bool is_scroll = (event->detail == XCB_BUTTON_SCROLL_UP
                    || event->detail == XCB_BUTTON_SCROLL_DOWN
                    || event->detail == XCB_BUTTON_SCROLL_LEFT
                    || event->detail == XCB_BUTTON_SCROLL_RIGHT);

    /* 1: see if the user scrolled on the decoration of a stacked/tabbed con */
    if (in_stacked && dest == CLICK_DECORATION && is_scroll) {
        DEBUG("Scrolling on a window decoration");
        /* Correctly move workspace focus first, see: #5472 */
        workspace_show(ws);

        GWMContainer* current = TAILQ_FIRST(&(con->parent->focusHead));
        const GWMPosition direction = (event->detail == XCB_BUTTON_SCROLL_UP || event->detail == XCB_BUTTON_SCROLL_LEFT) ? BEFORE : AFTER;
        GWMContainer* next = tree_get_tree_next_sibling(current, direction);
        container_activate(container_descend_focused(next ? next : current));

        allow_replay_pointer(event->time);
        return;
    }

    /* 2: floating modifier pressed, initiate a drag */
    if (mod_pressed
        && is_left_click
        && !floatingcon
        && (gConfig.tilingDrag == TILING_DRAG_MODIFIER || gConfig.tilingDrag == TILING_DRAG_MODIFIER_OR_TITLEBAR)
        && tiling_drag_has_drop_targets()) {
        const bool use_threshold = !mod_pressed;
        tiling_drag(con, event, use_threshold);
        allow_replay_pointer(event->time);
        return;
    }

    /* 3: focus this con or one of its children. */
    GWMContainer* con_to_focus = con;
    if (in_stacked && dest == CLICK_DECORATION) {
        if (was_focused || !container_has_parent(gFocused, con)) {
            while (!TAILQ_EMPTY(&(con_to_focus->focusHead))) {
                con_to_focus = TAILQ_FIRST(&(con_to_focus->focusHead));
            }
        }
    }
    if (ws != focused_workspace) {
        workspace_show(ws);
    }
    container_activate(con_to_focus);

    /* 4: For floating containers, we also want to raise them on click.
     * We will skip handling events on floating cons in fullscreen mode */
    GWMContainer* fs = container_get_full_screen_covering_ws(ws);
    if (floatingcon != NULL && fs != con) {
        /* 5: floating_modifier plus left mouse button drags */
        if (mod_pressed && is_left_click) {
            floating_drag_window(floatingcon, event, false);
            return;
        }

        /*  6: resize (floating) if this was a (left or right) click on the
         * left/right/bottom border, or a right click on the decoration.
         * also try resizing (tiling) if possible */
        if (mod_pressed && is_right_click) {
            DEBUG("floating resize due to floatingmodifier");
            floating_resize_window(floatingcon, proportional, event);
            return;
        }

        if ((dest == CLICK_BORDER || dest == CLICK_DECORATION) &&
            is_left_or_right_click) {
            /* try tiling resize, but continue if it doesn’t work */
            DEBUG("tiling resize with fallback");
            if (tiling_resize(con, event, dest, dest == CLICK_DECORATION && !was_focused)) {
                allow_replay_pointer(event->time);
                return;
            }
        }

        if (dest == CLICK_DECORATION && is_right_click) {
            DEBUG("floating resize due to decoration right click");
            floating_resize_window(floatingcon, proportional, event);
            return;
        }

        if (dest == CLICK_BORDER && is_left_or_right_click) {
            DEBUG("floating resize due to border click");
            floating_resize_window(floatingcon, proportional, event);
            return;
        }

        /* 7: dragging, if this was a click on a decoration (which did not lead
         * to a resize) */
        if (dest == CLICK_DECORATION && is_left_click) {
            floating_drag_window(floatingcon, event, !was_focused);
            return;
        }

        allow_replay_pointer(event->time);
        return;
    }

    /* 8: floating modifier pressed, or click in titlebar, initiate a drag */
    if (is_left_click
        && ((gConfig.tilingDrag == TILING_DRAG_TITLEBAR && dest == CLICK_DECORATION)
            || (gConfig.tilingDrag == TILING_DRAG_MODIFIER_OR_TITLEBAR && (mod_pressed
            || dest == CLICK_DECORATION)))
        && tiling_drag_has_drop_targets()) {
        allow_replay_pointer(event->time);
        const bool use_threshold = !mod_pressed;
        tiling_drag(con, event, use_threshold);
        return;
    }

    /* 9: floating modifier pressed, initiate a resize */
    if (dest == CLICK_INSIDE && mod_pressed && is_right_click) {
        if (floating_mod_on_tiled_client(con, event)) {
            return;
        }
        /* Avoid propagating events to clients, since the user expects
         * $mod+click to be handled by i3. */
        xcb_allow_events(gConn, XCB_ALLOW_ASYNC_POINTER, event->time);
        xcb_flush(gConn);
        return;
    }
    /* 10: otherwise, check for border/decoration clicks and resize */
    if ((dest == CLICK_BORDER || dest == CLICK_DECORATION) && is_left_or_right_click) {
        DEBUG("Trying to resize (tiling)");
        tiling_resize(con, event, dest, dest == CLICK_DECORATION && !was_focused);
    }

    allow_replay_pointer(event->time);
}

static bool tiling_resize(GWMContainer* con, xcb_button_press_event_t *event, const click_destination_t dest, bool use_threshold)
{
    /* check if this was a click on the window border (and on which one) */
    GWMRect bsr = container_border_style_rect(con);
    DEBUG("BORDER x = %d, y = %d for con %p, window 0x%08x", event->event_x, event->event_y, con, event->event);
    DEBUG("checks for right >= %d", con->windowRect.x + con->windowRect.width);
    if (dest == CLICK_DECORATION) {
        return tiling_resize_for_border(con, BORDER_TOP, event, use_threshold);
    }
    else if (dest == CLICK_BORDER) {
        if (event->event_y >= 0 && event->event_y <= (int32_t)bsr.y
            && event->event_x >= (int32_t)bsr.x && event->event_x <= (int32_t)(con->rect.width + bsr.width)) {
            return tiling_resize_for_border(con, BORDER_TOP, event, false);
        }
    }
    if (event->event_x >= 0 && event->event_x <= (int32_t)bsr.x
        && event->event_y >= (int32_t)bsr.y && event->event_y <= (int32_t)(con->rect.height + bsr.height)) {
        return tiling_resize_for_border(con, BORDER_LEFT, event, false);
    }

    if (event->event_x >= (int32_t)(con->windowRect.x + con->windowRect.width)
        && event->event_y >= (int32_t)bsr.y && event->event_y <= (int32_t)(con->rect.height + bsr.height)) {
        return tiling_resize_for_border(con, BORDER_RIGHT, event, false);
    }

    if (event->event_y >= (int32_t)(con->windowRect.y + con->windowRect.height)) {
        return tiling_resize_for_border(con, BORDER_BOTTOM, event, false);
    }

    return false;
}

static void allow_replay_pointer(xcb_timestamp_t time)
{
    xcb_allow_events(gConn, XCB_ALLOW_REPLAY_POINTER, time);
    xcb_flush(gConn);
    tree_render();
}

static bool floating_mod_on_tiled_client(GWMContainer* con, xcb_button_press_event_t *event)
{
    int to_right = con->rect.width - event->event_x,
        to_left = event->event_x,
        to_top = event->event_y,
        to_bottom = con->rect.height - event->event_y;

    DEBUG("click was %d px to the right, %d px to the left, %d px to top, %d px to bottom", to_right, to_left, to_top, to_bottom);

    if (to_right < to_left && to_right < to_top && to_right < to_bottom)
        return tiling_resize_for_border(con, BORDER_RIGHT, event, false);

    if (to_left < to_right && to_left < to_top && to_left < to_bottom)
        return tiling_resize_for_border(con, BORDER_LEFT, event, false);

    if (to_top < to_right && to_top < to_left && to_top < to_bottom)
        return tiling_resize_for_border(con, BORDER_TOP, event, false);

    if (to_bottom < to_right && to_bottom < to_left && to_bottom < to_top)
        return tiling_resize_for_border(con, BORDER_BOTTOM, event, false);

    return false;
}

static bool tiling_resize_for_border(GWMContainer* con, GWMBorder border, xcb_button_press_event_t *event, bool use_threshold)
{
    DEBUG("border = %d, con = %p", border, con);
    GWMContainer *second = NULL;
    GWMContainer *first = con;
    GWMDirection search_direction;
    switch (border) {
        case BORDER_LEFT:
            search_direction = D_LEFT;
            break;
        case BORDER_RIGHT:
            search_direction = D_RIGHT;
            break;
        case BORDER_TOP:
            search_direction = D_UP;
            break;
        case BORDER_BOTTOM:
            search_direction = D_DOWN;
            break;
        default: {
            ERROR("BUG: invalid border value %d", border);
            return false;
        }
    }

    bool res = resize_find_tiling_participants(&first, &second, search_direction, false);
    if (!res) {
        DEBUG("No second container in this direction found.");
        return false;
    }
    if (first->fullScreenMode != second->fullScreenMode) {
        DEBUG("Avoiding resize between containers with different fullscreen modes, %d != %d", first->fullScreenMode, second->fullScreenMode);
        return false;
    }

    g_assert(first != second);
    g_assert(first->parent == second->parent);

    /* The first container should always be in front of the second container */
    if (search_direction == D_UP || search_direction == D_LEFT) {
        GWMContainer* tmp = first;
        first = second;
        second = tmp;
    }

    const GWMOrientation orientation = ((border == BORDER_LEFT || border == BORDER_RIGHT) ? HORIZON : VERT);

    resize_graphical_handler(first, second, orientation, event, use_threshold);

    DEBUG("After resize handler, rendering");
    tree_render();

    return true;
}

static void handle_map_request(xcb_map_request_event_t *event)
{
    xcb_get_window_attributes_cookie_t cookie;

    cookie = xcb_get_window_attributes_unchecked(gConn, event->window);

    DEBUG("window = 0x%08x, serial is %d.", event->window, event->sequence);
    handler_add_ignore_event(event->sequence, -1);

    manage_window(event->window, cookie, false);
}

static void handle_unmap_notify_event(xcb_unmap_notify_event_t *event)
{
    DEBUG("UnmapNotify for 0x%08x (received from 0x%08x), serial %d", event->window, event->event, event->sequence);
    xcb_get_input_focus_cookie_t cookie;
    GWMContainer* con = container_by_window_id(event->window);
    if (con == NULL) {
        con = container_by_frame_id(event->window);
        if (con == NULL) {
            DEBUG("Not a managed window, ignoring UnmapNotify event");
            return;
        }

        if (con->ignoreUnmap > 0) {
            con->ignoreUnmap--;
        }

        cookie = xcb_get_input_focus(gConn);
        DEBUG("ignore_unmap = %d for frame of container %p", con->ignoreUnmap, con);
        goto ignore_end;
    }

    cookie = xcb_get_input_focus(gConn);
    if (con->ignoreUnmap > 0) {
        DEBUG("ignore_unmap = %d, dec", con->ignoreUnmap);
        con->ignoreUnmap--;
        goto ignore_end;
    }

    xcb_delete_property(gConn, event->window, A__NET_WM_DESKTOP);
    xcb_delete_property(gConn, event->window, A__NET_WM_STATE);

    tree_close_internal(con, KILL_WINDOW_DO_NOT, false);
    tree_render();

ignore_end:
    handler_add_ignore_event(event->sequence, XCB_ENTER_NOTIFY);
    xcb_get_input_focus_reply_t* resp = xcb_get_input_focus_reply(gConn, cookie, NULL);
    FREE(resp);
}

static void handle_destroy_notify_event(xcb_destroy_notify_event_t *event)
{
    DEBUG("destroy notify for 0x%08x, 0x%08x", event->event, event->window);

    xcb_unmap_notify_event_t unmap;
    unmap.sequence = event->sequence;
    unmap.event = event->event;
    unmap.window = event->window;

    handle_unmap_notify_event(&unmap);
}

static void handle_motion_notify(xcb_motion_notify_event_t *event)
{
    gLastTimestamp = event->time;

    if (event->child != XCB_NONE) {
        return;
    }

    GWMContainer* con;
    if ((con = container_by_frame_id(event->event)) == NULL) {
        DEBUG("MotionNotify for an unknown container, checking if it crosses screen boundaries.");
        check_crossing_screen_boundary(event->root_x, event->root_y);
        return;
    }

    if (gConfig.disableFocusFollowsMouse) {
        return;
    }

    if (con->layout != L_DEFAULT && con->layout != L_SPLIT_V && con->layout != L_SPLIT_H) {
        return;
    }

    if (con->window != NULL) {
        if (util_rect_contains(con->decorationRect, event->event_x, event->event_y)) {
            if (TAILQ_FIRST(&(con->parent->focusHead)) == con) {
                return;
            }
            container_focus(con);
            x_push_changes(gContainerRoot);
            return;
        }
    }
    else {
        GWMContainer* current;
        TAILQ_FOREACH_REVERSE (current, &(con->nodesHead), nodesHead, nodes) {
            if (!util_rect_contains(current->decorationRect, event->event_x, event->event_y)) {
                continue;
            }

            if (TAILQ_FIRST(&(con->focusHead)) == current) {
                return;
            }

            container_focus(current);
            x_push_changes(gContainerRoot);
            return;
        }
    }
}

static void check_crossing_screen_boundary(uint32_t x, uint32_t y)
{
    GWMOutput *output;

    if (gConfig.disableFocusFollowsMouse) {
        return;
    }

    if ((output = randr_get_output_containing(x, y)) == NULL) {
        ERROR("ERROR: No such screen");
        return;
    }

    if (output->container == NULL) {
        ERROR("ERROR: The screen is not recognized by gwm (no container associated)");
        return;
    }

    GWMContainer* old_focused = gFocused;
    GWMContainer* next = container_descend_focused(output_get_content(output->container));
    workspace_show(container_get_workspace(next));
    container_focus(next);

    if (old_focused != gFocused) {
        tree_render();
    }
}

static void handle_enter_notify(xcb_enter_notify_event_t *event)
{
    GWMContainer* con;
    gLastTimestamp = event->time;

    DEBUG("enter_notify for %08x, mode = %d, detail %d, serial %d", event->event, event->mode, event->detail, event->sequence);
    DEBUG("coordinates %d, %d", event->event_x, event->event_y);
    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        DEBUG("This was not a normal notify, ignoring");
        return;
    }
    if (handler_event_is_ignored(event->sequence, XCB_ENTER_NOTIFY)) {
        DEBUG("Event ignored");
        return;
    }

    bool enter_child = false;
    if ((con = container_by_frame_id(event->event)) == NULL) {
        con = container_by_window_id(event->event);
        enter_child = true;
    }

    if (con == NULL || con->parent->type == CT_DOCK_AREA) {
        DEBUG("Getting screen at %d x %d", event->root_x, event->root_y);
        check_crossing_screen_boundary(event->root_x, event->root_y);
        return;
    }

    GWMLayout layout = (enter_child ? con->parent->layout : con->layout);
    if (layout == L_DEFAULT) {
        GWMContainer* child;
        TAILQ_FOREACH_REVERSE (child, &(con->nodesHead), nodesHead, nodes) {
            if (util_rect_contains(child->decorationRect, event->event_x, event->event_y)) {
                DEBUG("using child %p / %s instead!", child, child->name);
                con = child;
                break;
            }
        }
    }

    if (gConfig.disableFocusFollowsMouse) {
        return;
    }

    if (con == gFocused) {
        return;
    }

    GWMContainer* ws = container_get_workspace(con);
    if (ws != container_get_workspace(gFocused)) {
        workspace_show(ws);
    }

    gFocusedID = XCB_NONE;
    container_focus(container_descend_focused(con));
    tree_render();
}

static void handle_client_message(xcb_client_message_event_t *event)
{
    if (sn_xcb_display_process_event(gSnDisplay, (xcb_generic_event_t *)event)) {
        return;
    }

    INFO("ClientMessage for window 0x%08x", event->window);
    if (event->type == A__NET_WM_STATE) {
        if (event->format != 32
            || (event->data.data32[1] != A__NET_WM_STATE_FULLSCREEN
                && event->data.data32[1] != A__NET_WM_STATE_DEMANDS_ATTENTION
                && event->data.data32[1] != A__NET_WM_STATE_STICKY)) {
            DEBUG("Unknown atom in clientmessage of type %d", event->data.data32[1]);
            return;
        }

        GWMContainer* con = container_by_window_id(event->window);
        if (con == NULL) {
            DEBUG("Could not get window for client message");
            return;
        }

        if (event->data.data32[1] == A__NET_WM_STATE_FULLSCREEN) {
            if ((con->fullScreenMode!= CF_NONE && (event->data.data32[0] == _NET_WM_STATE_REMOVE || event->data.data32[0] == _NET_WM_STATE_TOGGLE))
                || (con->fullScreenMode == CF_NONE && (event->data.data32[0] == _NET_WM_STATE_ADD || event->data.data32[0] == _NET_WM_STATE_TOGGLE))) {
                DEBUG("toggling fullscreen");
                container_toggle_full_screen(con, CF_OUTPUT);
            }
        }
        else if (event->data.data32[1] == A__NET_WM_STATE_DEMANDS_ATTENTION) {
            if (event->data.data32[0] == _NET_WM_STATE_ADD) {
                container_set_urgency(con, true);
                con = manage_remanage_window(con);
            } else if (event->data.data32[0] == _NET_WM_STATE_REMOVE) {
                container_set_urgency(con, false);
                con = manage_remanage_window(con);
            } else if (event->data.data32[0] == _NET_WM_STATE_TOGGLE) {
                container_set_urgency(con, !con->urgent);
                con = manage_remanage_window(con);
            }
        }
        else if (event->data.data32[1] == A__NET_WM_STATE_STICKY) {
            DEBUG("Received a client message to modify _NET_WM_STATE_STICKY.");
            if (event->data.data32[0] == _NET_WM_STATE_ADD)
                con->sticky = true;
            else if (event->data.data32[0] == _NET_WM_STATE_REMOVE)
                con->sticky = false;
            else if (event->data.data32[0] == _NET_WM_STATE_TOGGLE)
                con->sticky = !con->sticky;

            DEBUG("New sticky status for con = %p is %i.", con, con->sticky);
            extend_wm_hint_update_sticky(con->window->id, con->sticky);
            output_push_sticky_windows(gFocused);
            extend_wm_hint_update_wm_desktop();
        }

        tree_render();
    }
    else if (event->type == A__NET_ACTIVE_WINDOW) {
        if (event->format != 32) {
            return;
        }

        DEBUG("_NET_ACTIVE_WINDOW: Window 0x%08x should be activated", event->window);

        GWMContainer* con = container_by_window_id(event->window);
        if (con == NULL) {
            DEBUG("Could not get window for client message");
            return;
        }

        GWMContainer* ws = container_get_workspace(con);
        if (ws == NULL) {
            DEBUG("Window is not being managed, ignoring _NET_ACTIVE_WINDOW");
            return;
        }

        if (container_is_internal(ws) && ws != workspace_get("__gwm_scratch")) {
            DEBUG("Workspace is internal but not scratchpad, ignoring _NET_ACTIVE_WINDOW");
            return;
        }

        /* data32[0] indicates the source of the request (application or pager) */
        if (event->data.data32[0] == 2) {
            /* Always focus the con if it is from a pager, because this is most
             * likely from some user action */
            DEBUG("This request came from a pager. Focusing con = %p", con);

            if (container_is_internal(ws)) {
                scratchpad_show(con);
            }
            else {
                workspace_show(ws);
                gFocusedID = XCB_NONE;
                container_activate_unblock(con);
            }
        }
        else {
            if (container_is_internal(ws)) {
                DEBUG("Ignoring request to make con = %p active because it's on an internal workspace.", con);
                return;
            }

            if (gConfig.focusOnWindowActivation == FOWA_FOCUS || (gConfig.focusOnWindowActivation == FOWA_SMART && workspace_is_visible(ws))) {
                DEBUG("Focusing con = %p", con);
                container_activate_unblock(con);
            }
            else if (gConfig.focusOnWindowActivation == FOWA_URGENT || (gConfig.focusOnWindowActivation == FOWA_SMART && !workspace_is_visible(ws))) {
                DEBUG("Marking con = %p urgent", con);
                container_set_urgency(con, true);
                con = manage_remanage_window(con);
            }
            else {
                DEBUG("Ignoring request for con = %p.", con);
            }
        }
        tree_render();
    } else if (event->type == A_GWM_SYNC) {
        xcb_window_t window = event->data.data32[0];
        uint32_t rnd = event->data.data32[1];
        sync_respond(window, rnd);
    }
    else if (event->type == A__NET_REQUEST_FRAME_EXTENTS) {
        DEBUG("_NET_REQUEST_FRAME_EXTENTS for window 0x%08x", event->window);

        GWMRect r = {
            gConfig.defaultBorderWidth, /* left */
            gConfig.defaultBorderWidth, /* right */
            render_deco_height(),       /* top */
            gConfig.defaultBorderWidth  /* bottom */
        };
        xcb_change_property(
            gConn,
            XCB_PROP_MODE_REPLACE,
            event->window,
            A__NET_FRAME_EXTENTS,
            XCB_ATOM_CARDINAL, 32, 4,
            &r);
        xcb_flush(gConn);
    }
    else if (event->type == A_WM_CHANGE_STATE) {
        if (event->data.data32[0] == XCB_ICCCM_WM_STATE_ICONIC) {
            DEBUG("Client has requested iconic state, rejecting. (window = %08x)", event->window);
            long data[] = {XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE};
            xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, event->window, A_WM_STATE, A_WM_STATE, 32, 2, data);
        }
        else {
            DEBUG("Not handling WM_CHANGE_STATE request. (window = %08x, state = %d)", event->window, event->data.data32[0]);
        }
    }
    else if (event->type == A__NET_CURRENT_DESKTOP) {
        DEBUG("Request to change current desktop to index %d", event->data.data32[0]);
        GWMContainer* ws = extend_wm_hint_get_workspace_by_index(event->data.data32[0]);
        if (ws == NULL) {
            ERROR("Could not determine workspace for this index, ignoring request.");
            return;
        }

        DEBUG("Handling request to focus workspace %s", ws->name);
        workspace_show(ws);
        tree_render();
    }
    else if (event->type == A__NET_WM_DESKTOP) {
        uint32_t index = event->data.data32[0];
        DEBUG("Request to move window %d to EWMH desktop index %d", event->window, index);

        GWMContainer* con = container_by_window_id(event->window);
        if (con == NULL) {
            DEBUG("Couldn't find con for window %d, ignoring the request.", event->window);
            return;
        }

        if (index == NET_WM_DESKTOP_ALL) {
            DEBUG("The window was requested to be visible on all workspaces, making it sticky and floating.");

            if (floating_enable(con, false)) {
                con->floating = FLOATING_AUTO_ON;

                con->sticky = true;
                extend_wm_hint_update_sticky(con->window->id, true);
                output_push_sticky_windows(gFocused);
            }
        }
        else {
            GWMContainer* ws = extend_wm_hint_get_workspace_by_index(index);
            if (ws == NULL) {
                ERROR("Could not determine workspace for this index, ignoring request.");
                return;
            }
            container_move_to_workspace(con, ws, true, false, false);
        }

        tree_render();
        extend_wm_hint_update_wm_desktop();
    }
    else if (event->type == A__NET_CLOSE_WINDOW) {
        GWMContainer* con = container_by_window_id(event->window);
        if (con) {
            DEBUG("Handling _NET_CLOSE_WINDOW request (con = %p)", con);
            if (event->data.data32[0]) {
                gLastTimestamp = event->data.data32[0];
            }
            tree_close_internal(con, KILL_WINDOW, false);
            tree_render();
        }
        else {
            DEBUG("Couldn't find con for _NET_CLOSE_WINDOW request. (window = %08x)", event->window);
        }
    }
    else if (event->type == A__NET_WM_MOVERESIZE) {
        GWMContainer* con = container_by_window_id(event->window);
        if (!con || !container_is_floating(con)) {
            DEBUG("Couldn't find con for _NET_WM_MOVERESIZE request, or con not floating (window = %08x)", event->window);
            return;
        }
        DEBUG("Handling _NET_WM_MOVERESIZE request (con = %p)", con);
        uint32_t direction = event->data.data32[2];
        uint32_t x_root = event->data.data32[0];
        uint32_t y_root = event->data.data32[1];
        /* construct fake xcb_button_press_event_t */
        xcb_button_press_event_t fake = {
            .root_x = x_root,
            .root_y = y_root,
            .event_x = x_root - (con->rect.x),
            .event_y = y_root - (con->rect.y)};
        switch (direction) {
            case _NET_WM_MOVERESIZE_MOVE:
                floating_drag_window(con->parent, &fake, false);
                break;
            case _NET_WM_MOVERESIZE_SIZE_TOPLEFT ... _NET_WM_MOVERESIZE_SIZE_LEFT:
                floating_resize_window(con->parent, false, &fake);
                break;
            default:
                DEBUG("_NET_WM_MOVERESIZE direction %d not implemented", direction);
                break;
        }
    } else if (event->type == A__NET_MOVERESIZE_WINDOW) {
        DEBUG("Received _NET_MOVE_RESIZE_WINDOW. Handling by faking a configure request.");

        void *_generated_event = calloc(32, 1);
        xcb_configure_request_event_t *generated_event = _generated_event;

        generated_event->window = event->window;
        generated_event->response_type = XCB_CONFIGURE_REQUEST;

        generated_event->value_mask = 0;
        if (event->data.data32[0] & _NET_MOVERESIZE_WINDOW_X) {
            generated_event->value_mask |= XCB_CONFIG_WINDOW_X;
            generated_event->x = event->data.data32[1];
        }
        if (event->data.data32[0] & _NET_MOVERESIZE_WINDOW_Y) {
            generated_event->value_mask |= XCB_CONFIG_WINDOW_Y;
            generated_event->y = event->data.data32[2];
        }
        if (event->data.data32[0] & _NET_MOVERESIZE_WINDOW_WIDTH) {
            generated_event->value_mask |= XCB_CONFIG_WINDOW_WIDTH;
            generated_event->width = event->data.data32[3];
        }
        if (event->data.data32[0] & _NET_MOVERESIZE_WINDOW_HEIGHT) {
            generated_event->value_mask |= XCB_CONFIG_WINDOW_HEIGHT;
            generated_event->height = event->data.data32[4];
        }

        handle_configure_request(generated_event);
        FREE(generated_event);
    } else {
        DEBUG("Skipping client message for unhandled type %d", event->type);
    }
}

static void handle_screen_change(xcb_generic_event_t *e)
{
    DEBUG("RandR screen change");

    /* The geometry of the root window is used for “fullscreen global” and
     * changes when new outputs are added. */
    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(gConn, gRoot);
    xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(gConn, cookie, NULL);
    if (reply == NULL) {
        ERROR("Could not get geometry of the root window, exiting");
        exit(EXIT_FAILURE);
    }
    DEBUG("root geometry reply: (%d, %d) %d x %d", reply->x, reply->y, reply->width, reply->height);

    gContainerRoot->rect.width = reply->width;
    gContainerRoot->rect.height = reply->height;

    randr_query_outputs();

    scratchpad_fix_resolution();

//    ipc_send_event("output", GWM_IPC_EVENT_OUTPUT, "{\"change\":\"unspecified\"}");
}

static void handle_configure_request(xcb_configure_request_event_t *event)
{
    GWMContainer* con;

    DEBUG("window 0x%08x wants to be at %dx%d with %dx%d", event->window, event->x, event->y, event->width, event->height);

    if ((con = container_by_window_id(event->window)) == NULL) {
        DEBUG("Configure request for unmanaged window, can do that.");

        uint32_t mask = 0;
        uint32_t values[7];
        int c = 0;
#define COPY_MASK_MEMBER(mask_member, event_member) \
    do {                                            \
        if (event->value_mask & mask_member) {      \
            mask |= mask_member;                    \
            values[c++] = event->event_member;      \
        }                                           \
    } while (0)

        COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_X, x);
        COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_Y, y);
        COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_WIDTH, width);
        COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_HEIGHT, height);
        COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_BORDER_WIDTH, border_width);
        COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_SIBLING, sibling);
        COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_STACK_MODE, stack_mode);

        xcb_configure_window(gConn, event->window, mask, values);
        xcb_flush(gConn);
        return;
    }

    DEBUG("Configure request!");

    GWMContainer* workspace = container_get_workspace(con);
    if (workspace && (strcmp(workspace->name, "__gwm_scratch") == 0)) {
        DEBUG("This is a scratchpad container, ignoring ConfigureRequest");
        goto out;
    }
    GWMContainer* fullscreen = container_get_full_screen_covering_ws(workspace);

    if (fullscreen != con && container_is_floating(con) && container_is_leaf(con)) {
        GWMRect bsr = container_border_style_rect(con);
        GWMContainer* floatingcon = con->parent;
        GWMRect newrect = floatingcon->rect;

        if (event->value_mask & XCB_CONFIG_WINDOW_X) {
            newrect.x = event->x + (-1) * bsr.x;
            DEBUG("proposed x = %d, new x is %d", event->x, newrect.x);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_Y) {
            newrect.y = event->y + (-1) * bsr.y;
            DEBUG("proposed y = %d, new y is %d", event->y, newrect.y);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
            newrect.width = event->width + (-1) * bsr.width;
            newrect.width += con->borderWidth * 2;
            DEBUG("proposed width = %d, new width is %d (x11 border %d)", event->width, newrect.width, con->borderWidth);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            newrect.height = event->height + (-1) * bsr.height;
            newrect.height += con->borderWidth * 2;
            DEBUG("proposed height = %d, new height is %d (x11 border %d)", event->height, newrect.height, con->borderWidth);
        }

        DEBUG("Container is a floating leaf node, will do that.");
        floating_reposition(floatingcon, newrect);
        return;
    }

    /* Dock windows can be reconfigured in their height and moved to another output. */
    if (con->parent && con->parent->type == CT_DOCK_AREA) {
        DEBUG("Reconfiguring dock window (con = %p).", con);
        if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            DEBUG("Dock client wants to change height to %d, we can do that.", event->height);

            con->geoRect.height = event->height;
            tree_render();
        }

        if (event->value_mask & XCB_CONFIG_WINDOW_X || event->value_mask & XCB_CONFIG_WINDOW_Y) {
            int16_t x = event->value_mask & XCB_CONFIG_WINDOW_X ? event->x : (int16_t)con->geoRect.x;
            int16_t y = event->value_mask & XCB_CONFIG_WINDOW_Y ? event->y : (int16_t)con->geoRect.y;

            GWMContainer* current_output = container_get_output(con);
            GWMOutput *target = randr_get_output_containing(x, y);
            if (target != NULL && current_output != target->container) {
                DEBUG("Dock client is requested to be moved to output %s, moving it there.", output_primary_name(target));
                GWMMatch *match;
                GWMContainer* nc = container_for_window(target->container, con->window, &match);
                DEBUG("Dock client will be moved to container %p.", nc);
                container_detach(con);
                container_attach(con, nc, false);
                tree_render();
            }
            else {
                DEBUG("Dock client will not be moved, we only support moving it to another output.");
            }
        }
        goto out;
    }

    if (event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        DEBUG("window 0x%08x wants to be stacked %d", event->window, event->stack_mode);
        if (event->stack_mode != XCB_STACK_MODE_ABOVE) {
            DEBUG("stack_mode != XCB_STACK_MODE_ABOVE, ignoring ConfigureRequest");
            goto out;
        }

        if (fullscreen || !container_is_leaf(con)) {
            DEBUG("fullscreen or not a leaf, ignoring ConfigureRequest");
            goto out;
        }

        if (workspace == NULL) {
            DEBUG("Window is not being managed, ignoring ConfigureRequest");
            goto out;
        }

        if (gConfig.focusOnWindowActivation == FOWA_FOCUS || (gConfig.focusOnWindowActivation == FOWA_SMART && workspace_is_visible(workspace))) {
            DEBUG("Focusing con = %p", con);
            workspace_show(workspace);
            container_activate_unblock(con);
            tree_render();
        }
        else if (gConfig.focusOnWindowActivation == FOWA_URGENT || (gConfig.focusOnWindowActivation == FOWA_SMART && !workspace_is_visible(workspace))) {
            DEBUG("Marking con = %p urgent", con);
            container_set_urgency(con, true);
            con = manage_remanage_window(con);
            tree_render();
        }
        else {
            DEBUG("Ignoring request for con = %p.", con);
        }
    }

out:
    xcb_gwm_fake_absolute_configure_notify(con);
}

static void handle_mapping_notify(xcb_mapping_notify_event_t *event)
{
    if (event->request != XCB_MAPPING_KEYBOARD && event->request != XCB_MAPPING_MODIFIER) {
        return;
    }

    DEBUG("Received mapping_notify for keyboard or modifier mapping, re-grabbing keys");
    xcb_refresh_keyboard_mapping(gKeySyms, event);

    gXCBNumLockMask = util_aio_get_mod_mask_for(XCB_NUM_LOCK, gKeySyms);

    config_ungrab_all_keys(gConn);
    key_binding_translate_keysyms();
    key_binding_grab_all_keys(gConn);
}

static void handle_focus_in(xcb_focus_in_event_t *event)
{
    DEBUG("focus change in, for window 0x%08x", event->event);

    if (event->event == gRoot) {
        DEBUG("Received focus in for root window, refocusing the focused window.");
        container_focus(gFocused);
        gFocusedID = XCB_NONE;
        x_push_changes(gContainerRoot);
    }

    GWMContainer *con;
    if ((con = container_by_window_id(event->event)) == NULL || con->window == NULL) {
        return;
    }
    DEBUG("That is con %p / %s", con, con->name);

    if (event->mode == XCB_NOTIFY_MODE_GRAB || event->mode == XCB_NOTIFY_MODE_UNGRAB) {
        DEBUG("FocusIn event for grab/ungrab, ignoring");
        return;
    }

    if (event->detail == XCB_NOTIFY_DETAIL_POINTER) {
        DEBUG("notify detail is pointer, ignoring this event");
        return;
    }

    if (gFocusedID == event->event && !container_inside_floating(con)) {
        DEBUG("focus matches the currently focused window, not doing anything");
        return;
    }

    /* Skip dock clients, they cannot get the i3 focus. */
    if (con->parent->type == CT_DOCK_AREA) {
        DEBUG("This is a dock client, not focusing.");
        return;
    }

    DEBUG("focus is different / refocusing floating window: updating decorations");

    container_activate_unblock(con);

    gFocusedID = event->event;
    tree_render();
}

static void handle_focus_out(xcb_focus_in_event_t *event)
{
    GWMContainer* con = container_by_window_id(event->event);
    const char *window_name, *mode, *detail;

    if (con != NULL) {
        window_name = con->name;
        if (window_name == NULL) {
            window_name = "<unnamed con>";
        }
    } else if (event->event == gRoot) {
        window_name = "<the root window>";
    } else {
        window_name = "<unknown window>";
    }

    switch (event->mode) {
        case XCB_NOTIFY_MODE_NORMAL:
            mode = "Normal";
            break;
        case XCB_NOTIFY_MODE_GRAB:
            mode = "Grab";
            break;
        case XCB_NOTIFY_MODE_UNGRAB:
            mode = "Ungrab";
            break;
        case XCB_NOTIFY_MODE_WHILE_GRABBED:
            mode = "WhileGrabbed";
            break;
        default:
            mode = "<unknown>";
            break;
    }

    switch (event->detail) {
        case XCB_NOTIFY_DETAIL_ANCESTOR:
            detail = "Ancestor";
            break;
        case XCB_NOTIFY_DETAIL_VIRTUAL:
            detail = "Virtual";
            break;
        case XCB_NOTIFY_DETAIL_INFERIOR:
            detail = "Inferior";
            break;
        case XCB_NOTIFY_DETAIL_NONLINEAR:
            detail = "Nonlinear";
            break;
        case XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL:
            detail = "NonlinearVirtual";
            break;
        case XCB_NOTIFY_DETAIL_POINTER:
            detail = "Pointer";
            break;
        case XCB_NOTIFY_DETAIL_POINTER_ROOT:
            detail = "PointerRoot";
            break;
        case XCB_NOTIFY_DETAIL_NONE:
            detail = "NONE";
            break;
        default:
            detail = "unknown";
            break;
    }

    DEBUG("focus change out: window 0x%08x (con %p, %s) lost focus with detail=%s, mode=%s", event->event, con, window_name, detail, mode);
}

static void handle_configure_notify(xcb_configure_notify_event_t *event)
{
    if (event->event != gRoot) {
        DEBUG("ConfigureNotify for non-root window 0x%08x, ignoring", event->event);
        return;
    }
    DEBUG("ConfigureNotify for root window 0x%08x", event->event);

//    if (gForceXinerama) {
        return;
//    }
//    randr_query_outputs();

//    ipc_send_event("output", GWM_IPC_EVENT_OUTPUT, "{\"change\":\"unspecified\"}");
}

static void property_notify(uint8_t state, xcb_window_t window, xcb_atom_t atom)
{
    GWMPropertyHandler* handler = NULL;
    xcb_get_property_reply_t *propr = NULL;
    xcb_generic_error_t *err = NULL;
    GWMContainer* con;

    for (size_t c = 0; c < G_N_ELEMENTS(gPropertyHandlers); c++) {
        if (gPropertyHandlers[c].atom != atom) {
            continue;
        }
        handler = &gPropertyHandlers[c];
        break;
    }

    if (handler == NULL) {
        /* DEBUG("Unhandled property notify for atom %d (0x%08x)", atom, atom); */
        return;
    }

    if ((con = container_by_window_id(window)) == NULL || con->window == NULL) {
        DEBUG("Received property for atom %d for unknown client", atom);
        return;
    }

    if (state != XCB_PROPERTY_DELETE) {
        xcb_get_property_cookie_t cookie = xcb_get_property(gConn, 0, window, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, handler->longLen);
        propr = xcb_get_property_reply(gConn, cookie, &err);
        if (err != NULL) {
            DEBUG("got error %d when getting property of atom %d", err->error_code, atom);
            FREE(err);
            return;
        }
    }

    if (!handler->cb(con, propr)) {
        FREE(propr);
    }
}

static void handle_selection_clear(xcb_selection_clear_event_t *event)
{
    if (event->selection != gWMSn) {
        DEBUG("SelectionClear for unknown selection %d, ignoring", event->selection);
        return;
    }
    DEBUG("Lost WM_Sn selection, exiting.");
    exit(EXIT_SUCCESS);

    /* unreachable */
}