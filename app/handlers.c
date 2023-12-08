//
// Created by dingjing on 23-11-24.
//

#include "handlers.h"

#include <glib.h>
#include <time.h>
#include <sys/time.h>

#include <xcb/randr.h>
#include <libsn/sn-monitor.h>

#include "x.h"
#include "val.h"
#include "log.h"
#include "tree.h"
#include "manage.h"
#include "window.h"
#include "startup.h"
#include "floating.h"
#include "container.h"
#include "draw-util.h"


typedef struct GWMPropertyHandler           GWMPropertyHandler;
typedef bool (*GWMPropertyHandlerCB) (GWMContainer* con, xcb_get_property_reply_t* property);


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



static GSList   gIgnoreEvents;
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

}

bool handler_event_is_ignored(int sequence, int responseType)
{
    return 0;
}

void handler_handle_event(int type, xcb_generic_event_t *event)
{
    DEBUG("handle event: %d", type);
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
        INFO("expose event for unknown window, ignoring\n");
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
        free (reply);
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