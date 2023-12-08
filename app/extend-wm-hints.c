//
// Created by dingjing on 23-11-24.
//

#include "extend-wm-hints.h"

#include <xcb/xcb.h>
#include <glib/gi18n.h>

#include "log.h"
#include "val.h"
#include "xcb.h"
#include "output.h"
#include "container.h"
#include "workspace.h"
#include "xmacro-atoms_NET-SUPPORTED.h"


#define FOREACH_NONINTERNAL \
    TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) \
        TAILQ_FOREACH (ws, &(output_get_content(output)->nodesHead), nodes) \
            if (!container_is_internal(ws))


static void extend_wm_hint_update_desktop_names();
static void extend_wm_hint_update_desktop_viewport(void);
static void extend_wm_hint_update_number_of_desktops(void);
static void extend_wm_hint_update_wm_desktop_recursively (GWMContainer* con, uint32_t desktop);


void extend_wm_hint_setup_hint(void)
{
    xcb_atom_t supportedAtoms[] = {
#define GWM_ATOM_MACRO(atom) A_##atom,
        GWM_NET_SUPPORTED_ATOMS_XMACRO
#undef GWM_ATOM_MACRO
    };

    gExtendWMHintsWindow = xcb_generate_id (gConn);

    xcb_create_window (gConn, XCB_COPY_FROM_PARENT, gExtendWMHintsWindow, gRoot, -1, -1, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_OVERRIDE_REDIRECT, (uint32_t[]) {1});

    xcb_change_property (gConn, XCB_PROP_MODE_REPLACE, gExtendWMHintsWindow, A__NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 32, 1, &gExtendWMHintsWindow);
    xcb_change_property (gConn, XCB_PROP_MODE_REPLACE, gExtendWMHintsWindow, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen (_(PACKAGE_NAME)), _(PACKAGE_NAME));
    xcb_change_property (gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 32, 1, &gExtendWMHintsWindow);
    xcb_change_property (gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen (_(PACKAGE_NAME)), _(PACKAGE_NAME));
    xcb_change_property (gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_SUPPORTED, XCB_ATOM_ATOM, 32, sizeof(supportedAtoms) / sizeof (xcb_atom_t), supportedAtoms);
    xcb_map_window (gConn, gExtendWMHintsWindow);
    xcb_configure_window (gConn, gExtendWMHintsWindow, XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t[]) {XCB_STACK_MODE_BELOW});

}

void extend_wm_hint_update_desktop_properties(void)
{
    extend_wm_hint_update_number_of_desktops();
    extend_wm_hint_update_desktop_viewport();
    extend_wm_hint_update_current_desktop();
    extend_wm_hint_update_desktop_names();
    extend_wm_hint_update_wm_desktop();
}

void extend_wm_hint_update_current_desktop(void)
{
    static uint32_t oldIdx = NET_WM_DESKTOP_NONE;
    const uint32_t idx = extend_wm_hint_get_workspace_index(gFocused);

    if (idx == oldIdx || idx == NET_WM_DESKTOP_NONE) {
        return;
    }

    oldIdx = idx;

    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_CURRENT_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &idx);
}

void extend_wm_hint_update_wm_desktop(void)
{
    uint32_t desktop = 0;

    GWMContainer* output = NULL;
    TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
        GWMContainer* workspace;
        TAILQ_FOREACH (workspace, &(output_get_content(output)->nodesHead), nodes) {
            extend_wm_hint_update_wm_desktop_recursively(workspace, desktop);
            if (!container_is_internal(workspace)) {
                ++desktop;
            }
        }
    }
}

void extend_wm_hint_update_active_window(xcb_window_t window)
{
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 32, 1, &window);
}

void extend_wm_hint_update_visible_name(xcb_window_t window, const char *name)
{
    if (name != NULL) {
        xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, window, A__NET_WM_VISIBLE_NAME, A_UTF8_STRING, 8, strlen(name), name);
    }
    else {
        xcb_delete_property(gConn, window, A__NET_WM_VISIBLE_NAME);
    }
}

void extend_wm_hint_update_client_list(xcb_window_t *list, int numWindows)
{
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_CLIENT_LIST, XCB_ATOM_WINDOW, 32, numWindows, list);
}

void extend_wm_hint_update_client_list_stacking(xcb_window_t *stack, int numWindows)
{
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_CLIENT_LIST_STACKING, XCB_ATOM_WINDOW, 32, numWindows, stack);
}

void extend_wm_hint_update_sticky(xcb_window_t window, bool sticky)
{
    if (sticky) {
        DEBUG(_("Setting _NET_WM_STATE_STICKY for window = %08x."), window);
        xcb_gwm_add_property_atom(gConn, window, A__NET_WM_STATE, A__NET_WM_STATE_STICKY);
    }
    else {
        DEBUG(_("Removing _NET_WM_STATE_STICKY for window = %08x."), window);
        xcb_gwm_remove_property_atom(gConn, window, A__NET_WM_STATE, A__NET_WM_STATE_STICKY);
    }
}

void extend_wm_hint_update_focused(xcb_window_t window, bool isFocused)
{
    if (isFocused) {
        DEBUG(_("Setting _NET_WM_STATE_FOCUSED for window = %08x."), window);
        xcb_gwm_add_property_atom (gConn, window, A__NET_WM_STATE, A__NET_WM_STATE_FOCUSED);
    }
    else {
        DEBUG(_("Removing _NET_WM_STATE_FOCUSED for window = %08x."), window);
        xcb_gwm_remove_property_atom (gConn, window, A__NET_WM_STATE, A__NET_WM_STATE_FOCUSED);
    }
}

void extend_wm_hint_update_work_area(void)
{
    xcb_delete_property (gConn, gRoot, A__NET_WORKAREA);
}

uint32_t extend_wm_hint_get_workspace_index(GWMContainer *con)
{
    uint32_t idx = 0;
    GWMContainer* ws = NULL;
    GWMContainer* output = NULL;
    GWMContainer* targetWorkspace = container_get_workspace (con);

    FOREACH_NONINTERNAL {
        if (ws == targetWorkspace) {
            return idx;
        }
        idx++;
    }

    return NET_WM_DESKTOP_NONE;
}

GWMContainer* extend_wm_hint_get_workspace_by_index(uint32_t idx)
{
    if (NET_WM_DESKTOP_NONE == idx) {
        return NULL;
    }

    uint32_t currentIdx = 0;
    GWMContainer* ws = NULL;
    GWMContainer* output = NULL;

    FOREACH_NONINTERNAL {
        if (currentIdx == idx) {
            return ws;
        }
        currentIdx++;
    }

    return NULL;
}


static void extend_wm_hint_update_wm_desktop_recursively (GWMContainer* con, uint32_t desktop)
{
    GWMContainer* child = NULL;

    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        extend_wm_hint_update_wm_desktop_recursively(child, desktop);
    }

    /* If con is a workspace, we also need to go through the floating windows on it. */
    if (con->type == CT_WORKSPACE) {
        TAILQ_FOREACH (child, &(con->floatingHead), floatingWindows) {
            extend_wm_hint_update_wm_desktop_recursively(child, desktop);
        }
    }

    if (!container_has_managed_window(con)) {
        return;
    }

    uint32_t wmDesktop = desktop;
    if (container_is_sticky(con) && container_is_floating(con)) {
        wmDesktop = NET_WM_DESKTOP_ALL;
    }

    GWMContainer* ws = container_get_workspace(con);
    if (ws != NULL && container_is_internal(ws)) {
        wmDesktop = NET_WM_DESKTOP_ALL;
    }

    if (con->window->wmDesktop == wmDesktop) {
        return;
    }
    con->window->wmDesktop = wmDesktop;

    const xcb_window_t window = con->window->id;
    if (wmDesktop != NET_WM_DESKTOP_NONE) {
        DEBUG(_("Setting _NET_WM_DESKTOP = %d for window 0x%08x."), wmDesktop, window);
        xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, window, A__NET_WM_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &wmDesktop);
    }
    else {
        DEBUG(_("Failed to determine the proper EWMH desktop index for window 0x%08x, deleting _NET_WM_DESKTOP."), window);
        xcb_delete_property(gConn, window, A__NET_WM_DESKTOP);
    }
}

static void extend_wm_hint_update_desktop_viewport(void)
{
    int numDesktops = 0;
    GWMContainer* ws = NULL;
    GWMContainer* output = NULL;

    FOREACH_NONINTERNAL {
        numDesktops++;
    }

    uint32_t viewports[numDesktops * 2];

    int currentPosition = 0;
    FOREACH_NONINTERNAL {
        viewports[currentPosition++] = output->rect.x;
        viewports[currentPosition++] = output->rect.y;
    }

    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_DESKTOP_VIEWPORT, XCB_ATOM_CARDINAL, 32, currentPosition, &viewports);
}

static void extend_wm_hint_update_number_of_desktops(void)
{
    uint32_t idx = 0;
    static uint32_t oldIdx = 0;
    GWMContainer* ws = NULL;
    GWMContainer* output = NULL;

    FOREACH_NONINTERNAL {
        idx++;
    };

    if (idx == oldIdx) {
        return;
    }
    oldIdx = idx;

    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_NUMBER_OF_DESKTOPS, XCB_ATOM_CARDINAL, 32, 1, &idx);
}

static void extend_wm_hint_update_desktop_names()
{
    int msgLen = 0;
    GWMContainer* ws = NULL;
    GWMContainer* output = NULL;

    FOREACH_NONINTERNAL {
        msgLen += strlen(ws->name) + 1;
    };


    char desktopNames[msgLen];
    int currentPosition = 0;

    /* fill the buffer with the names of the i3 workspaces */
    FOREACH_NONINTERNAL {
        for (size_t i = 0; i < strlen(ws->name) + 1; i++) {
            desktopNames[currentPosition++] = ws->name[i];
        }
    }

    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A__NET_DESKTOP_NAMES, A_UTF8_STRING, 8, msgLen, desktopNames);
}
