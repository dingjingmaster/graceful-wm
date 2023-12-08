//
// Created by dingjing on 23-11-24.
//

#include "x.h"

#include "log.h"
#include "val.h"
#include "xcb.h"
#include "draw-util.h"


typedef struct GWMContainerState
{
    xcb_window_t            id;
    bool                    mapped;
    bool                    unmapNow;
    bool                    childMapped;
    bool                    isHidden;

    /* The con for which this state is. */
    GWMContainer*           con;

    /* For reparenting, we have a flag (need_reparent) and the X ID of the old
     * frame this window was in. The latter is necessary because we need to
     * ignore UnmapNotify events (by changing the window event mask). */
    bool                    needReparent;
    xcb_window_t            oldFrame;

    /* The container was child of floating container during the previous call of
     * x_push_node(). This is used to remove the shape when the container is no
     * longer floating. */
    bool                    wasFloating;

    GWMRect                 rect;
    GWMRect                 windowRect;

    bool                    initial;
    char*                   name;

    GQueue                  state;
    GQueue                  oldState;
    GQueue                  initialMappingOrder;
//    CIRCLEQ_ENTRY(con_state) state;
//    CIRCLEQ_ENTRY(con_state) old_state;
//    TAILQ_ENTRY(con_state) initial_mapping_order;
} GWMContainerState;

GQueue          gStateHead = G_QUEUE_INIT;
GQueue          gOldStateHead = G_QUEUE_INIT;
GQueue          gInitialMappingHead = G_QUEUE_INIT;

void x_container_init(GWMContainer *con)
{
    /* TODO: maybe create the window when rendering first? we could then even
     * get the initial geometry right */

    uint32_t mask = 0;
    uint32_t values[5];

    xcb_visualid_t visual = xcb_gwm_get_visualid_by_depth(con->depth);
    xcb_colormap_t winColormap;
    if (con->depth != gRootDepth) {
        /* We need to create a custom colormap. */
        winColormap = xcb_generate_id(gConn);
        xcb_create_colormap(gConn, XCB_COLORMAP_ALLOC_NONE, winColormap, gRoot, visual);
        con->colormap = winColormap;
    }
    else {
        /* Use the default colormap. */
        winColormap = gColormap;
        con->colormap = XCB_NONE;
    }

    /* We explicitly set a background color and border color (even though we
     * don’t even have a border) because the X11 server requires us to when
     * using 32 bit color depths, see
     * https://stackoverflow.com/questions/3645632 */
    mask |= XCB_CW_BACK_PIXEL;
    values[0] = gRootScreen->black_pixel;

    mask |= XCB_CW_BORDER_PIXEL;
    values[1] = gRootScreen->black_pixel;

    /* our own frames should not be managed */
    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[2] = 1;

    /* see include/xcb.h for the FRAME_EVENT_MASK */
    mask |= XCB_CW_EVENT_MASK;
    values[3] = FRAME_EVENT_MASK & ~XCB_EVENT_MASK_ENTER_WINDOW;

    mask |= XCB_CW_COLORMAP;
    values[4] = winColormap;

    GWMRect dims = {-15, -15, 10, 10};
    xcb_window_t frame_id = xcb_gwm_create_window(gConn, dims, con->depth, visual, XCB_WINDOW_CLASS_INPUT_OUTPUT, CURSOR_POINTER, false, mask, values);
    draw_util_surface_init(gConn, &(con->frame), frame_id, xcb_gwm_get_visual_type_by_id(visual), dims.width, dims.height);
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, con->frame.id, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, (strlen("graceful-wm-frame") + 1) * 2, "graceful-wm-frame\0graceful-wm-frame\0");

    GWMContainerState* state = g_malloc0 (sizeof(GWMContainerState));
    state->id = con->frame.id;
    state->mapped = false;
    state->initial = true;
    DEBUG("Adding window 0x%08x to lists\n", state->id);

    g_queue_push_head (&gStateHead, state);
    g_queue_push_head (&gOldStateHead, state);

//    CIRCLEQ_INSERT_HEAD(&state_head, state, state);
//    CIRCLEQ_INSERT_HEAD(&old_state_head, state, old_state);
//    TAILQ_INSERT_TAIL(&initial_mapping_head, state, initial_mapping_order);

    DEBUG("adding new state for window id 0x%08x", state->id);
}

void x_move_window(GWMContainer *src, GWMContainer *destination)
{

}

void x_reparent_child(GWMContainer *con, GWMContainer *old)
{

}

void x_reinit(GWMContainer *con)
{

}

void x_container_kill(GWMContainer *con)
{

}

bool x_window_supports_protocol(xcb_window_t window, xcb_atom_t atom)
{
    return 0;
}

void x_container_reframe(GWMContainer *con)
{

}

void x_draw_decoration(GWMContainer *con)
{

}

void x_decoration_recurse(GWMContainer *con)
{

}

void x_push_node(GWMContainer *con)
{

}

void x_push_changes(GWMContainer *con)
{

}

void x_raise_container(GWMContainer *con)
{

}

void x_set_name(GWMContainer *con, const char *name)
{

}

void x_set_gwm_atoms(void)
{

}

void x_set_warp_to(GWMRect *rect)
{

}

void x_set_shape(GWMContainer *con, xcb_shape_sk_t kind, bool enable)
{

}

void x_window_kill(xcb_window_t window, GWMKillWindow killWindow)
{

}
