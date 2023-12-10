//
// Created by dingjing on 23-11-24.
//

#include <unistd.h>
#include "x.h"

#include "log.h"
#include "val.h"
#include "xcb.h"
#include "draw-util.h"
#include "extend-wm-hints.h"
#include "utils.h"
#include "dpi.h"
#include "font.h"
#include "container.h"
#include "randr.h"


typedef struct ContainerState
{
    xcb_window_t                    id;
    bool                            mapped;
    bool                            unmapNow;
    bool                            childMapped;
    bool                            isHidden;
    GWMContainer*                   con;

    /* For reparenting, we have a flag (need_reparent) and the X ID of the old
     * frame this window was in. The latter is necessary because we need to
     * ignore UnmapNotify events (by changing the window event mask). */
    bool                            needReparent;
    xcb_window_t                    oldFrame;

    /* The container was child of floating container during the previous call of
     * x_push_node(). This is used to remove the shape when the container is no
     * longer floating. */
    bool                            wasFloating;
    GWMRect                         rect;
    GWMRect                         windowRect;
    bool                            initial;
    char*                           name;

    CIRCLEQ_ENTRY(ContainerState)   state;
    CIRCLEQ_ENTRY(ContainerState)   oldState;
    TAILQ_ENTRY(ContainerState)     initialMappingOrder;
} GWMContainerState;


static void _x_con_kill(GWMContainer* con);
static bool is_con_attached(GWMContainer* con);
static void set_hidden_state(GWMContainer* con);
static void x_push_node_unmaps(GWMContainer* con);
static void set_shape_state(GWMContainer* con, bool need_reshape);
static struct ContainerState* state_for_frame(xcb_window_t window);
bool window_supports_protocol(xcb_window_t window, xcb_atom_t atom);
static void x_shape_frame(GWMContainer* con, xcb_shape_sk_t shape_kind);
static void x_unshape_frame(GWMContainer* con, xcb_shape_sk_t shape_kind);
static void change_ewmh_focus(xcb_window_t new_focus, xcb_window_t old_focus);
static size_t x_get_border_rectangles(GWMContainer* con, xcb_rectangle_t rectangles[4]);
static void x_draw_title_border(GWMContainer* con, GWMDecorationRenderParams* p, GWMSurface* destSurface);
static void x_draw_decoration_after_title(GWMContainer* con, GWMDecorationRenderParams* p, GWMSurface* dest_surface);


static GWMRect*                                     gWarpTo = NULL;
static xcb_window_t                                 gLastFocused = XCB_NONE;
CIRCLEQ_HEAD(stateHead, ContainerState)             gStateHead = CIRCLEQ_HEAD_INITIALIZER(gStateHead);
CIRCLEQ_HEAD(oldStateHead, ContainerState)          gOldStateHead = CIRCLEQ_HEAD_INITIALIZER(gOldStateHead);
TAILQ_HEAD(initialMappingHead, ContainerState)      gInitialMappingHead = TAILQ_HEAD_INITIALIZER(gInitialMappingHead);


void x_container_init(GWMContainer *con)
{
    /* TODO: maybe create the window when rendering first? we could then even
     * get the initial geometry right */

    uint32_t mask = 0;
    uint32_t values[5];

    xcb_visualid_t visual = xcb_gwm_get_visualid_by_depth(con->depth);
    xcb_colormap_t winColormap;
    if (con->depth != gRootDepth) {
        winColormap = xcb_generate_id(gConn);
        xcb_create_colormap(gConn, XCB_COLORMAP_ALLOC_NONE, winColormap, gRoot, visual);
        con->colormap = winColormap;
    }
    else {
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

    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[2] = 1;

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
    DEBUG(_("Adding window 0x%08x to lists"), state->id);

    CIRCLEQ_INSERT_HEAD(&gStateHead, state, state);
    CIRCLEQ_INSERT_HEAD(&gOldStateHead, state, oldState);
    TAILQ_INSERT_TAIL(&gInitialMappingHead, state, initialMappingOrder);

    DEBUG("adding new state for window id 0x%08x", state->id);
}

void x_move_window(GWMContainer *src, GWMContainer *dest)
{
    GWMContainerState* state_src, *state_dest;

    if ((state_src = state_for_frame(src->frame.id)) == NULL) {
        ERROR("window state for src not found\n");
        return;
    }

    if ((state_dest = state_for_frame(dest->frame.id)) == NULL) {
        ERROR("window state for dest not found\n");
        return;
    }

    state_dest->con = state_src->con;
    state_src->con = NULL;

    if (util_rect_equals(state_dest->windowRect, (GWMRect){0, 0, 0, 0})) {
        memcpy(&(state_dest->windowRect), &(state_src->windowRect), sizeof(GWMRect));
        DEBUG("COPYING RECT");
    }
}

void x_reparent_child(GWMContainer *con, GWMContainer *old)
{
    GWMContainerState* state;
    if ((state = state_for_frame(con->frame.id)) == NULL) {
        ERROR("window state for con not found");
        return;
    }

    state->needReparent = true;
    state->oldFrame = old->frame.id;
}

void x_reinit(GWMContainer *con)
{
    GWMContainerState* state;

    if ((state = state_for_frame(con->frame.id)) == NULL) {
        ERROR("window state not found");
        return;
    }

    DEBUG("resetting state %p to initial", state);
    state->initial = true;
    state->childMapped = false;
    state->con = con;
    memset(&(state->windowRect), 0, sizeof(GWMRect));
}

void x_container_kill(GWMContainer *con)
{
    _x_con_kill(con);
    xcb_destroy_window(gConn, con->frame.id);
}

bool x_window_supports_protocol(xcb_window_t window, xcb_atom_t atom)
{
    return 0;
}

void x_container_reframe(GWMContainer *con)
{
    _x_con_kill(con);
    x_container_init(con);
}

void x_draw_decoration(GWMContainer *con)
{
    GWMContainer* parent = con->parent;
    bool leaf = container_is_leaf(con);

    if ((!leaf && parent->layout != L_STACKED && parent->layout != L_TABBED)
        || parent->type == CT_OUTPUT
        || parent->type == CT_DOCK_AREA
        || con->type == CT_FLOATING_CON) {
        return;
    }

    if (con->rect.height == 0) {
        return;
    }

    if (leaf && con->frameBuffer.id == XCB_NONE) {
        return;
    }

    GWMDecorationRenderParams* p = calloc(1, sizeof(GWMDecorationRenderParams));

#if 0
    if (con->urgent) {
        p->color = &config.client.urgent;
    } else if (con == focused || con_inside_focused(con)) {
        p->color = &config.client.focused;
    } else if (con == TAILQ_FIRST(&(parent->focus_head))) {
        if (config.client.got_focused_tab_title && !leaf && con_descend_focused(con) == focused) {
            /* Stacked/tabbed parent of focused container */
            p->color = &config.client.focused_tab_title;
        } else {
            p->color = &config.client.focused_inactive;
        }
    } else {
        p->color = &config.client.unfocused;
    }

    p->border_style = con_border_style(con);

    Rect *r = &(con->rect);
    Rect *w = &(con->window_rect);
    p->con_rect = (struct width_height){r->width, r->height};
    p->con_window_rect = (struct width_height){w->width, w->height};
    p->con_deco_rect = con->deco_rect;
    p->background = config.client.background;
    p->con_is_leaf = con_is_leaf(con);
    p->parent_layout = con->parent->layout;

    if (con->deco_render_params != NULL &&
        (con->window == NULL || !con->window->name_x_changed) &&
        !parent->pixmap_recreated &&
        !con->pixmap_recreated &&
        !con->mark_changed &&
        memcmp(p, con->deco_render_params, sizeof(struct deco_render_params)) == 0) {
        free(p);
        goto copy_pixmaps;
    }

    Con *next = con;
    while ((next = TAILQ_NEXT(next, nodes))) {
        FREE(next->deco_render_params);
    }

    FREE(con->deco_render_params);
    con->deco_render_params = p;

    if (con->window != NULL && con->window->name_x_changed)
        con->window->name_x_changed = false;

    parent->pixmap_recreated = false;
    con->pixmap_recreated = false;
    con->mark_changed = false;

    /* 2: draw the client.background, but only for the parts around the window_rect */
    if (con->window != NULL) {
        /* Clear visible windows before beginning to draw */
        draw_util_clear_surface(&(con->frame_buffer), (color_t){.red = 0.0, .green = 0.0, .blue = 0.0});

        /* top area */
        draw_util_rectangle(&(con->frame_buffer), config.client.background,
                            0, 0, r->width, w->y);
        /* bottom area */
        draw_util_rectangle(&(con->frame_buffer), config.client.background,
                            0, w->y + w->height, r->width, r->height - (w->y + w->height));
        /* left area */
        draw_util_rectangle(&(con->frame_buffer), config.client.background,
                            0, 0, w->x, r->height);
        /* right area */
        draw_util_rectangle(&(con->frame_buffer), config.client.background,
                            w->x + w->width, 0, r->width - (w->x + w->width), r->height);
    }

    /* 3: draw a rectangle in border color around the client */
    if (p->border_style != BS_NONE && p->con_is_leaf) {
        /* Fill the border. We don’t just fill the whole rectangle because some
         * children are not freely resizable and we want their background color
         * to "shine through". */
        xcb_rectangle_t rectangles[4];
        size_t rectangles_count = x_get_border_rectangles(con, rectangles);
        for (size_t i = 0; i < rectangles_count; i++) {
            draw_util_rectangle(&(con->frame_buffer), p->color->child_border,
                                rectangles[i].x,
                                rectangles[i].y,
                                rectangles[i].width,
                                rectangles[i].height);
        }

        /* Highlight the side of the border at which the next window will be
         * opened if we are rendering a single window within a split container
         * (which is undistinguishable from a single window outside a split
         * container otherwise. */
        Rect br = con_border_style_rect(con);
        if (TAILQ_NEXT(con, nodes) == NULL &&
            TAILQ_PREV(con, nodes_head, nodes) == NULL &&
            con->parent->type != CT_FLOATING_CON) {
            if (p->parent_layout == L_SPLITH) {
                draw_util_rectangle(&(con->frame_buffer), p->color->indicator,
                                    r->width + (br.width + br.x), br.y, -(br.width + br.x), r->height + br.height);
            } else if (p->parent_layout == L_SPLITV) {
                draw_util_rectangle(&(con->frame_buffer), p->color->indicator,
                                    br.x, r->height + (br.height + br.y), r->width + br.width, -(br.height + br.y));
            }
        }
    }

    surface_t *dest_surface = &(parent->frame_buffer);
    if (con_draw_decoration_into_frame(con)) {
        DLOG("using con->frame_buffer (for con->name=%s) as dest_surface\n", con->name);
        dest_surface = &(con->frame_buffer);
    } else {
        DLOG("sticking to parent->frame_buffer = %p\n", dest_surface);
    }
    DLOG("dest_surface %p is %d x %d (id=0x%08x)\n", dest_surface, dest_surface->width, dest_surface->height, dest_surface->id);

    /* If the parent hasn't been set up yet, skip the decoration rendering
     * for now. */
    if (dest_surface->id == XCB_NONE)
        goto copy_pixmaps;

    /* For the first child, we clear the parent pixmap to ensure there's no
     * garbage left on there. This is important to avoid tearing when using
     * transparency. */
    if (con == TAILQ_FIRST(&(con->parent->nodes_head))) {
        FREE(con->parent->deco_render_params);
    }

    /* if this is a borderless/1pixel window, we don’t need to render the
     * decoration. */
    if (p->border_style != BS_NORMAL)
        goto copy_pixmaps;

    /* 4: paint the bar */
    DLOG("con->deco_rect = (x=%d, y=%d, w=%d, h=%d) for con->name=%s\n",
         con->deco_rect.x, con->deco_rect.y, con->deco_rect.width, con->deco_rect.height, con->name);
    draw_util_rectangle(dest_surface, p->color->background,
                        con->deco_rect.x, con->deco_rect.y, con->deco_rect.width, con->deco_rect.height);

    /* 5: draw title border */
    x_draw_title_border(con, p, dest_surface);

    /* 6: draw the icon and title */
    int text_offset_y = (con->deco_rect.height - config.font.height) / 2;

    struct Window *win = con->window;

    const int deco_width = (int)con->deco_rect.width;
    const int title_padding = logical_px(2);

    int mark_width = 0;
    if (config.show_marks && !TAILQ_EMPTY(&(con->marks_head))) {
        char *formatted_mark = sstrdup("");
        bool had_visible_mark = false;

        mark_t *mark;
        TAILQ_FOREACH (mark, &(con->marks_head), marks) {
            if (mark->name[0] == '_')
                continue;
            had_visible_mark = true;

            char *buf;
            sasprintf(&buf, "%s[%s]", formatted_mark, mark->name);
            free(formatted_mark);
            formatted_mark = buf;
        }

        if (had_visible_mark) {
            i3String *mark = i3string_from_utf8(formatted_mark);
            mark_width = predict_text_width(mark);

            int mark_offset_x = (config.title_align == ALIGN_RIGHT)
                                ? title_padding
                                : deco_width - mark_width - title_padding;

            draw_util_text(mark, dest_surface,
                           p->color->text, p->color->background,
                           con->deco_rect.x + mark_offset_x,
                           con->deco_rect.y + text_offset_y, mark_width);
            I3STRING_FREE(mark);

            mark_width += title_padding;
        }

        FREE(formatted_mark);
    }

    i3String *title = NULL;
    if (win == NULL) {
        if (con->title_format == NULL) {
            char *_title;
            char *tree = con_get_tree_representation(con);
            sasprintf(&_title, "i3: %s", tree);
            free(tree);

            title = i3string_from_utf8(_title);
            FREE(_title);
        } else {
            title = con_parse_title_format(con);
        }
    } else {
        title = con->title_format == NULL ? win->name : con_parse_title_format(con);
    }
    if (title == NULL) {
        goto copy_pixmaps;
    }

    /* icon_padding is applied horizontally only, the icon will always use all
     * available vertical space. */
    int icon_size = max(0, con->deco_rect.height - logical_px(2));
    int icon_padding = logical_px(max(1, con->window_icon_padding));
    int total_icon_space = icon_size + 2 * icon_padding;
    const bool has_icon = (con->window_icon_padding > -1) && win && win->icon && (total_icon_space < deco_width);
    if (!has_icon) {
        icon_size = icon_padding = total_icon_space = 0;
    }
    /* Determine x offsets according to title alignment */
    int icon_offset_x;
    int title_offset_x;
    switch (config.title_align) {
        case ALIGN_LEFT:
            /* (pad)[(pad)(icon)(pad)][text    ](pad)[mark + its pad)
             *             ^           ^--- title_offset_x
             *             ^--- icon_offset_x */
            icon_offset_x = icon_padding;
            title_offset_x = title_padding + total_icon_space;
            break;
        case ALIGN_CENTER:
            /* (pad)[  ][(pad)(icon)(pad)][text  ](pad)[mark + its pad)
             *                 ^           ^--- title_offset_x
             *                 ^--- icon_offset_x
             * Text should come right after the icon (+padding). We calculate
             * the offset for the icon (white space in the title) by dividing
             * by two the total available area. That's the decoration width
             * minus the elements that come after icon_offset_x (icon, its
             * padding, text, marks). */
            icon_offset_x = max(icon_padding, (deco_width - icon_padding - icon_size - predict_text_width(title) - title_padding - mark_width) / 2);
            title_offset_x = max(title_padding, icon_offset_x + icon_padding + icon_size);
            break;
        case ALIGN_RIGHT:
            /* [mark + its pad](pad)[    text][(pad)(icon)(pad)](pad)
             *                           ^           ^--- icon_offset_x
             *                           ^--- title_offset_x */
            title_offset_x = max(title_padding + mark_width, deco_width - title_padding - predict_text_width(title) - total_icon_space);
            /* Make sure the icon does not escape title boundaries */
            icon_offset_x = min(deco_width - icon_size - icon_padding - title_padding, title_offset_x + predict_text_width(title) + icon_padding);
            break;
        default:
            ELOG("BUG: invalid config.title_align value %d\n", config.title_align);
            return;
    }

    draw_util_text(title, dest_surface,
                   p->color->text, p->color->background,
                   con->deco_rect.x + title_offset_x,
                   con->deco_rect.y + text_offset_y,
                   deco_width - mark_width - 2 * title_padding - total_icon_space);
    if (has_icon) {
        draw_util_image(
            win->icon,
            dest_surface,
            con->deco_rect.x + icon_offset_x,
            con->deco_rect.y + logical_px(1),
            icon_size,
            icon_size);
    }

    if (win == NULL || con->title_format != NULL) {
        I3STRING_FREE(title);
    }

    x_draw_decoration_after_title(con, p, dest_surface);
copy_pixmaps:
    draw_util_copy_surface(&(con->frame_buffer), &(con->frame), 0, 0, 0, 0, con->rect.width, con->rect.height);
#endif
}

void x_decoration_recurse(GWMContainer *con)
{
    GWMContainer* current;
    bool leaf = TAILQ_EMPTY(&(con->nodesHead)) && TAILQ_EMPTY(&(con->floatingHead));
    GWMContainerState* state = state_for_frame(con->frame.id);

    if (!leaf) {
        TAILQ_FOREACH (current, &(con->nodesHead), nodes) {
            x_decoration_recurse (current);
        }

        TAILQ_FOREACH (current, &(con->floatingHead), floatingWindows) {
            x_decoration_recurse(current);
        }

        if (state->mapped) {
            draw_util_copy_surface(&(con->frameBuffer), &(con->frame), 0, 0, 0, 0, con->rect.width, con->rect.height);
        }
    }

    if ((con->type != CT_ROOT && con->type != CT_OUTPUT) && (!leaf || con->mapped)) {
        x_draw_decoration(con);
    }
}

void x_push_node(GWMContainer *con)
{
    GWMContainer *current;
    GWMContainerState *state;
    GWMRect rect = con->rect;

    state = state_for_frame(con->frame.id);

    if (state->name != NULL) {
        DEBUG("pushing name %s for con %p\n", state->name, con);
        xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, con->frame.id, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(state->name), state->name);
        FREE(state->name);
    }

    if (con->window == NULL && (con->layout == L_STACKED || con->layout == L_TABBED)) {
        uint32_t max_y = 0, max_height = 0;
        TAILQ_FOREACH (current, &(con->nodesHead), nodes) {
            GWMRect *dr = &(current->decorationRect);
            if (dr->y >= max_y && dr->height >= max_height) {
                max_y = dr->y;
                max_height = dr->height;
            }
        }
        rect.height = max_y + max_height;
        if (rect.height == 0) {
            con->mapped = false;
        }
    }
    else if (con->window == NULL) {
        con->mapped = false;
    }

    bool need_reshape = false;

    if (state->needReparent && con->window != NULL) {
        DEBUG("Reparenting child window\n");
        uint32_t values[] = {XCB_NONE};
        xcb_change_window_attributes(gConn, state->oldFrame, XCB_CW_EVENT_MASK, values);
        xcb_change_window_attributes(gConn, con->window->id, XCB_CW_EVENT_MASK, values);
        xcb_reparent_window(gConn, con->window->id, con->frame.id, 0, 0);

        values[0] = FRAME_EVENT_MASK;
        xcb_change_window_attributes(gConn, state->oldFrame, XCB_CW_EVENT_MASK, values);
        values[0] = CHILD_EVENT_MASK;
        xcb_change_window_attributes(gConn, con->window->id, XCB_CW_EVENT_MASK, values);

        state->oldFrame = XCB_NONE;
        state->needReparent = false;

        con->ignoreUnmap++;
        DEBUG("ignore_unmap for reparenting of con %p (win 0x%08x) is now %d\n", con, con->window->id, con->ignoreUnmap);

        need_reshape = true;
    }

    need_reshape |= state->rect.width != rect.width ||
                    state->rect.height != rect.height ||
                    state->windowRect.width != con->windowRect.width ||
                    state->windowRect.height != con->windowRect.height;

    need_reshape |= container_is_floating(con) && !state->wasFloating;

    bool is_pixmap_needed = ((container_is_leaf(con) && container_border_style(con) != BS_NONE) || con->layout == L_STACKED || con->layout == L_TABBED);
    DEBUG("Con %p (layout %d), is_pixmap_needed = %s, rect.height = %d\n", con, con->layout, is_pixmap_needed ? "yes" : "no", con->rect.height);

    if (con->type == CT_ROOT || con->type == CT_OUTPUT) {
        is_pixmap_needed = false;
    }

    bool fake_notify = false;
    if ((is_pixmap_needed && con->frameBuffer.id == XCB_NONE) || (!util_rect_equals(state->rect, rect) && rect.height > 0)) {
        bool has_rect_changed = (state->rect.x != rect.x || state->rect.y != rect.y || state->rect.width != rect.width || state->rect.height != rect.height);
        if (!is_pixmap_needed && con->frameBuffer.id != XCB_NONE) {
            draw_util_surface_free(gConn, &(con->frameBuffer));
            xcb_free_pixmap(gConn, con->frameBuffer.id);
            con->frameBuffer.id = XCB_NONE;
        }

        if (is_pixmap_needed && (has_rect_changed || con->frameBuffer.id == XCB_NONE)) {
            if (con->frameBuffer.id == XCB_NONE) {
                con->frameBuffer.id = xcb_generate_id(gConn);
            }
            else {
                draw_util_surface_free(gConn, &(con->frameBuffer));
                xcb_free_pixmap(gConn, con->frameBuffer.id);
            }

            uint16_t win_depth = gRootDepth;
            if (con->window) {
                win_depth = con->window->depth;
            }

            int width = MAX((int32_t)rect.width, 1);
            int height = MAX((int32_t)rect.height, 1);

            DEBUG("creating %d x %d pixmap for con %p (con->frame_buffer.id = (pixmap_t)0x%08x) (con->frame.id (drawable_t)0x%08x)\n", width, height, con, con->frameBuffer.id, con->frame.id);
            xcb_create_pixmap(gConn, win_depth, con->frameBuffer.id, con->frame.id, width, height);
            draw_util_surface_init(gConn, &(con->frameBuffer), con->frameBuffer.id, xcb_gwm_get_visual_type_by_id(xcb_gwm_get_visualid_by_depth(win_depth)), width, height);
            draw_util_clear_surface(&(con->frameBuffer), (GWMColor ){.red = 0.0, .green = 0.0, .blue = 0.0});

            xcb_change_gc(gConn, con->frameBuffer.gc, XCB_GC_GRAPHICS_EXPOSURES, (uint32_t[]){0});

            draw_util_surface_set_size(&(con->frame), width, height);
            con->pixmapRecreated = true;

            if (!con->parent || con->parent->layout != L_STACKED || TAILQ_FIRST(&(con->parent->focusHead)) == con) {
                x_decoration_recurse (con);
            }
        }

        DEBUG("setting rect (%d, %d, %d, %d)\n", rect.x, rect.y, rect.width, rect.height);
        xcb_flush(gConn);
        xcb_gwm_set_window_rect(gConn, con->frame.id, rect);
        if (con->frameBuffer.id != XCB_NONE) {
            draw_util_copy_surface(&(con->frameBuffer), &(con->frame), 0, 0, 0, 0, con->rect.width, con->rect.height);
        }
        xcb_flush(gConn);

        memcpy(&(state->rect), &rect, sizeof(GWMRect));
        fake_notify = true;
    }

    if (con->window != NULL && !util_rect_equals(state->windowRect, con->windowRect)) {
        DEBUG("setting window rect (%d, %d, %d, %d)\n", con->windowRect.x, con->windowRect.y, con->windowRect.width, con->windowRect.height);
        xcb_gwm_set_window_rect(gConn, con->window->id, con->windowRect);
        memcpy(&(state->windowRect), &(con->windowRect), sizeof(GWMRect));
        fake_notify = true;
    }

    set_shape_state(con, need_reshape);

    if ((state->mapped != con->mapped || (con->window != NULL && !state->childMapped)) &&
        con->mapped) {
        xcb_void_cookie_t cookie;
        if (con->window != NULL) {
            long data[] = {XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE};
            xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, con->window->id, A_WM_STATE, A_WM_STATE, 32, 2, data);
        }

        uint32_t values[1];
        if (!state->childMapped && con->window != NULL) {
            cookie = xcb_map_window(gConn, con->window->id);
            values[0] = CHILD_EVENT_MASK;
            xcb_change_window_attributes(gConn, con->window->id, XCB_CW_EVENT_MASK, values);
            DEBUG("mapping child window (serial %d)\n", cookie.sequence);
            state->childMapped = true;
        }

        cookie = xcb_map_window(gConn, con->frame.id);

        values[0] = FRAME_EVENT_MASK;
        xcb_change_window_attributes(gConn, con->frame.id, XCB_CW_EVENT_MASK, values);

        if (con->frameBuffer.id != XCB_NONE) {
            draw_util_copy_surface(&(con->frameBuffer), &(con->frame), 0, 0, 0, 0, con->rect.width, con->rect.height);
        }
        xcb_flush(gConn);

        DEBUG("mapping container %08x (serial %d)\n", con->frame.id, cookie.sequence);
        state->mapped = con->mapped;
    }

    state->unmapNow = (state->mapped != con->mapped) && !con->mapped;
    state->wasFloating = container_is_floating(con);

    if (fake_notify) {
        DEBUG("Sending fake configure notify\n");
        xcb_gwm_fake_absolute_configure_notify(con);
    }

    set_hidden_state(con);

    /* Handle all children and floating windows of this node. We recurse
     * in focus order to display the focused client in a stack first when
     * switching workspaces (reduces flickering). */
    TAILQ_FOREACH (current, &(con->focusHead), focused) {
        x_push_node(current);
    }
}

void x_push_changes(GWMContainer *con)
{
    GWMContainerState *state;
    xcb_query_pointer_cookie_t pointercookie;

    if (gWarpTo) {
        pointercookie = xcb_query_pointer(gConn, gRoot);
    }

    DEBUG("-- PUSHING WINDOW STACK --\n");
    uint32_t values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
    CIRCLEQ_FOREACH_REVERSE (state, &gStateHead, state) {
        if (state->mapped) {
            xcb_change_window_attributes(gConn, state->id, XCB_CW_EVENT_MASK, values);
        }
    }
    bool order_changed = false;
    bool stacking_changed = false;

    int cnt = 0;
    CIRCLEQ_FOREACH_REVERSE (state, &gStateHead, state) {
        if (container_has_managed_window(state->con)) {
            cnt++;
        }
    }

    static xcb_window_t *client_list_windows = NULL;
    static int client_list_count = 0;

    if (cnt != client_list_count) {
        client_list_windows = realloc(client_list_windows, sizeof(xcb_window_t) * cnt);
        client_list_count = cnt;
    }

    xcb_window_t *walk = client_list_windows;

    /* X11 correctly represents the stack if we push it from bottom to top */
    CIRCLEQ_FOREACH_REVERSE (state, &gStateHead, state) {
        if (container_has_managed_window(state->con)) {
            memcpy(walk++, &(state->con->window->id), sizeof(xcb_window_t));
        }

        GWMContainerState *prev = CIRCLEQ_PREV(state, state);
        GWMContainerState *old_prev = CIRCLEQ_PREV(state, oldState);
        if (prev != old_prev)
            order_changed = true;
        if ((state->initial || order_changed) && prev != CIRCLEQ_END(&gStateHead)) {
            stacking_changed = true;
            uint32_t mask = 0;
            mask |= XCB_CONFIG_WINDOW_SIBLING;
            mask |= XCB_CONFIG_WINDOW_STACK_MODE;
            uint32_t values[] = {state->id, XCB_STACK_MODE_ABOVE};

            xcb_configure_window(gConn, prev->id, mask, values);
        }
        state->initial = false;
    }

    if (stacking_changed) {
        DEBUG("Client list changed (%i clients)\n", cnt);
        extend_wm_hint_update_client_list_stacking(client_list_windows, client_list_count);
        walk = client_list_windows;
        TAILQ_FOREACH (state, &gInitialMappingHead, initialMappingOrder) {
            if (container_has_managed_window(state->con)) {
                *walk++ = state->con->window->id;
            }
        }
        extend_wm_hint_update_client_list(client_list_windows, client_list_count);
    }

    DEBUG("PUSHING CHANGES\n");
    x_push_node(con);

    if (gWarpTo) {
        xcb_query_pointer_reply_t *pointerreply = xcb_query_pointer_reply(gConn, pointercookie, NULL);
        if (!pointerreply) {
            ERROR("Could not query pointer position, not warping pointer\n");
        }
        else {
            int mid_x = gWarpTo->x + (gWarpTo->width / 2);
            int mid_y = gWarpTo->y + (gWarpTo->height / 2);

            GWMOutput *current = randr_get_output_containing(pointerreply->root_x, pointerreply->root_y);
            GWMOutput *target = randr_get_output_containing(mid_x, mid_y);
            if (current != target) {
                xcb_change_window_attributes(gConn, gRoot, XCB_CW_EVENT_MASK, (uint32_t[]){XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT});
                xcb_warp_pointer(gConn, XCB_NONE, gRoot, 0, 0, 0, 0, mid_x, mid_y);
                xcb_change_window_attributes(gConn, gRoot, XCB_CW_EVENT_MASK, (uint32_t[]){ROOT_EVENT_MASK});
            }

            free(pointerreply);
        }
        gWarpTo = NULL;
    }

    values[0] = FRAME_EVENT_MASK;
    CIRCLEQ_FOREACH_REVERSE (state, &gStateHead, state) {
        if (state->mapped) {
            xcb_change_window_attributes(gConn, state->id, XCB_CW_EVENT_MASK, values);
        }
    }

    x_decoration_recurse (con);

    xcb_window_t to_focus = gFocused->frame.id;
    if (gFocused->window != NULL) {
        to_focus = gFocused->window->id;
    }

    if (gFocusedID != to_focus) {
        if (!gFocused->mapped) {
            DEBUG("Not updating focus (to %p / %s), focused window is not mapped.", gFocused, gFocused->name);
            gFocusedID = XCB_NONE;
        }
        else {
            if (gFocused->window != NULL && gFocused->window->needTakeFocus && gFocused->window->doesNotAcceptFocus) {
                DEBUG("Updating focus by sending WM_TAKE_FOCUS to window 0x%08x (focused: %p / %s)", to_focus, gFocused, gFocused->name);
                xcb_gwm_send_take_focus(to_focus, gLastTimestamp);
                change_ewmh_focus((container_has_managed_window(gFocused) ? gFocused->window->id : XCB_WINDOW_NONE), gLastFocused);
                if (to_focus != gLastFocused && is_con_attached(gFocused)) {
//                    ipc_send_window_event("focus", focused);
                }
            }
            else {
                DEBUG("Updating focus (focused: %p / %s) to X11 window 0x%08x\n", gFocused, gFocused->name, to_focus);
                if (gFocused->window != NULL) {
                    values[0] = CHILD_EVENT_MASK & ~(XCB_EVENT_MASK_FOCUS_CHANGE);
                    xcb_change_window_attributes(gConn, gFocused->window->id, XCB_CW_EVENT_MASK, values);
                }
                xcb_set_input_focus(gConn, XCB_INPUT_FOCUS_POINTER_ROOT, to_focus, gLastTimestamp);
                if (gFocused->window != NULL) {
                    values[0] = CHILD_EVENT_MASK;
                    xcb_change_window_attributes(gConn, gFocused->window->id, XCB_CW_EVENT_MASK, values);
                }

                change_ewmh_focus((container_has_managed_window(gFocused) ? gFocused->window->id : XCB_WINDOW_NONE), gLastFocused);
                if (to_focus != XCB_NONE && to_focus != gLastFocused && gFocused->window != NULL && is_con_attached(gFocused)) {
//                    ipc_send_window_event("focus", focused);
                }
            }
            gFocusedID = gLastFocused = to_focus;
        }
    }

    if (gFocusedID == XCB_NONE) {
        DEBUG("Still no window focused, better set focus to the EWMH support window (%d)\n", gExtendWMHintsWindow);
        xcb_set_input_focus(gConn, XCB_INPUT_FOCUS_POINTER_ROOT, gExtendWMHintsWindow, gLastTimestamp);
        change_ewmh_focus(XCB_WINDOW_NONE, gLastFocused);

        gFocusedID = gExtendWMHintsWindow;
        gLastFocused = XCB_NONE;
    }

    xcb_flush(gConn);
    DEBUG("ENDING CHANGES\n");

    values[0] = FRAME_EVENT_MASK & ~XCB_EVENT_MASK_ENTER_WINDOW;
    CIRCLEQ_FOREACH_REVERSE (state, &gStateHead, state) {
        if (!state->unmapNow) {
            continue;
        }
        xcb_change_window_attributes(gConn, state->id, XCB_CW_EVENT_MASK, values);
    }

    x_push_node_unmaps(con);

    CIRCLEQ_FOREACH (state, &gStateHead, state) {
        CIRCLEQ_REMOVE(&gOldStateHead, state, oldState);
        CIRCLEQ_INSERT_TAIL(&gOldStateHead, state, oldState);
    }

    xcb_flush(gConn);
}

void x_raise_container(GWMContainer *con)
{
    GWMContainerState * state;
    state = state_for_frame(con->frame.id);

    CIRCLEQ_REMOVE(&gStateHead, state, state);
    CIRCLEQ_INSERT_HEAD(&gStateHead, state, state);
}

void x_set_name(GWMContainer *con, const char *name)
{
    GWMContainerState *state;

    if ((state = state_for_frame(con->frame.id)) == NULL) {
        ERROR("window state not found");
        return;
    }

    FREE(state->name);
    state->name = g_strdup(name);
}

void x_set_gwm_atoms(void)
{
    pid_t pid = getpid();
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A_GWM_SOCKET_PATH, A_UTF8_STRING, 8, (gCurrentSocketPath == NULL ? 0 : strlen(gCurrentSocketPath)), gCurrentSocketPath);
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A_GWM_PID, XCB_ATOM_CARDINAL, 32, 1, &pid);
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A_GWM_CONFIG_PATH, A_UTF8_STRING, 8, strlen(gCurrentConfigPath), gCurrentConfigPath);
    xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A_GWM_LOG_STREAM_SOCKET_PATH, A_UTF8_STRING, 8, strlen(gCurrentLogStreamSocketPath), gCurrentLogStreamSocketPath);
    x_update_shmlog_atom();
}

void x_set_warp_to(GWMRect *rect)
{
//    if (config.mouse_warping != POINTER_WARPING_NONE)
//        gWarpTo = rect;
}

void x_set_shape(GWMContainer *con, xcb_shape_sk_t kind, bool enable)
{
    struct con_state *state;
    if ((state = state_for_frame(con->frame.id)) == NULL) {
        ERROR("window state for con %p not found\n", con);
        return;
    }

    switch (kind) {
        case XCB_SHAPE_SK_BOUNDING: {
            con->window->shaped = enable;
            break;
        }
        case XCB_SHAPE_SK_INPUT: {
            con->window->inputShaped = enable;
            break;
        }
        default: {
            ERROR("Received unknown shape event kind for con %p. This is a bug.\n", con);
            return;
        }
    }

    if (container_is_floating(con)) {
        if (enable) {
            x_shape_frame(con, kind);
        }
        else {
            x_unshape_frame(con, kind);
        }
        xcb_flush(gConn);
    }
}

void x_window_kill(xcb_window_t window, GWMKillWindow killWindow)
{
    if (!window_supports_protocol(window, A_WM_DELETE_WINDOW)) {
        if (killWindow == KILL_WINDOW) {
            INFO("Killing specific window 0x%08x", window);
            xcb_destroy_window(gConn, window);
        }
        else {
            INFO("Killing the X11 client which owns window 0x%08x", window);
            xcb_kill_client(gConn, window);
        }
        return;
    }

    void *event = calloc(32, 1);
    xcb_client_message_event_t *ev = event;

    ev->response_type = XCB_CLIENT_MESSAGE;
    ev->window = window;
    ev->type = A_WM_PROTOCOLS;
    ev->format = 32;
    ev->data.data32[0] = A_WM_DELETE_WINDOW;
    ev->data.data32[1] = XCB_CURRENT_TIME;

    INFO("Sending WM_DELETE to the client\n");
    xcb_send_event(gConn, false, window, XCB_EVENT_MASK_NO_EVENT, (char *)ev);
    xcb_flush(gConn);
    free(event);
}

void x_mask_event_mask(uint32_t mask)
{
    uint32_t values[] = {FRAME_EVENT_MASK & mask};

    GWMContainerState * state;
    CIRCLEQ_FOREACH_REVERSE (state, &gStateHead, state) {
        if (state->mapped) {
            xcb_change_window_attributes(gConn, state->id, XCB_CW_EVENT_MASK, values);
        }
    }
}

void x_update_shmlog_atom(void)
{
    if (*gShmLogName == '\0') {
        xcb_delete_property(gConn, gRoot, A_GWM_SHMLOG_PATH);
    }
    else {
        xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, gRoot, A_GWM_SHMLOG_PATH, A_UTF8_STRING, 8, strlen(gShmLogName), gShmLogName);
    }
}

static struct ContainerState* state_for_frame(xcb_window_t window)
{
    struct ContainerState* state;
    CIRCLEQ_FOREACH (state, &gStateHead, state) {
        if (state->id == window) {
            return state;
        }
    }

    /* TODO: better error handling? */
    ERROR("No state found for window 0x%08x", window);
    g_assert(false);

    return NULL;
}

static void change_ewmh_focus(xcb_window_t new_focus, xcb_window_t old_focus)
{
    if (new_focus == old_focus) {
        return;
    }

    extend_wm_hint_update_active_window(new_focus);

    if (new_focus != XCB_WINDOW_NONE) {
        extend_wm_hint_update_focused(new_focus, true);
    }

    if (old_focus != XCB_WINDOW_NONE) {
        extend_wm_hint_update_focused(old_focus, false);
    }
}

static void _x_con_kill(GWMContainer* con)
{
    GWMContainerState* state;

    if (con->colormap != XCB_NONE) {
        xcb_free_colormap(gConn, con->colormap);
    }

    draw_util_surface_free(gConn, &(con->frame));
    draw_util_surface_free(gConn, &(con->frameBuffer));
    xcb_free_pixmap(gConn, con->frameBuffer.id);
    con->frameBuffer.id = XCB_NONE;
    state = state_for_frame(con->frame.id);
    CIRCLEQ_REMOVE(&gStateHead, state, state);
    CIRCLEQ_REMOVE(&gOldStateHead, state, oldState);
    TAILQ_REMOVE(&gInitialMappingHead, state, initialMappingOrder);
    FREE(state->name);
    free(state);

    /* Invalidate focused_id to correctly focus new windows with the same ID */
    if (con->frame.id == gFocusedID) {
        gFocusedID = XCB_NONE;
    }
    if (con->frame.id == gLastFocused) {
        gLastFocused = XCB_NONE;
    }
}

bool window_supports_protocol(xcb_window_t window, xcb_atom_t atom)
{
    xcb_get_property_cookie_t cookie;
    xcb_icccm_get_wm_protocols_reply_t protocols;
    bool result = false;

    cookie = xcb_icccm_get_wm_protocols(gConn, window, A_WM_PROTOCOLS);
    if (xcb_icccm_get_wm_protocols_reply(gConn, cookie, &protocols, NULL) != 1)
        return false;

    for (uint32_t i = 0; i < protocols.atoms_len; i++) {
        if (protocols.atoms[i] == atom) {
            result = true;
        }
    }

    xcb_icccm_get_wm_protocols_reply_wipe(&protocols);

    return result;
}

static void x_draw_title_border(GWMContainer* con, GWMDecorationRenderParams* p, GWMSurface* destSurface)
{
    GWMRect* dr = &(con->decorationRect);

    /* Left */
    draw_util_rectangle(destSurface, p->color->border, dr->x, dr->y, 1, dr->height);

    /* Right */
    draw_util_rectangle(destSurface, p->color->border, dr->x + dr->width - 1, dr->y, 1, dr->height);

    /* Top */
    draw_util_rectangle(destSurface, p->color->border, dr->x, dr->y, dr->width, 1);

    /* Bottom */
    draw_util_rectangle(destSurface, p->color->border, dr->x, dr->y + dr->height - 1, dr->width, 1);
}

static void x_draw_decoration_after_title(GWMContainer* con, GWMDecorationRenderParams* p, GWMSurface* dest_surface)
{
    g_assert(con->parent != NULL);

    GWMRect *dr = &(con->decorationRect);

    if (!font_is_pango()) {
        draw_util_rectangle(dest_surface, p->color->background, dr->x + dr->width - 2 * dpi_logical_px(1), dr->y, 2 * dpi_logical_px(1), dr->height);
    }

    x_draw_title_border(con, p, dest_surface);
}

static size_t x_get_border_rectangles(GWMContainer* con, xcb_rectangle_t rectangles[4])
{
    size_t count = 0;
    int border_style = container_border_style(con);

    if (border_style != BS_NONE && container_is_leaf(con)) {
        GWMAdjacent borders_to_hide = container_adjacent_borders(con);
        GWMRect br = container_border_style_rect(con);

        if (!(borders_to_hide & ADJ_LEFT_SCREEN_EDGE)) {
            rectangles[count++] = (xcb_rectangle_t){
                .x = 0,
                .y = 0,
                .width = br.x,
                .height = con->rect.height,
            };
        }
        if (!(borders_to_hide & ADJ_RIGHT_SCREEN_EDGE)) {
            rectangles[count++] = (xcb_rectangle_t){
                .x = con->rect.width + (br.width + br.x),
                .y = 0,
                .width = -(br.width + br.x),
                .height = con->rect.height,
            };
        }
        if (!(borders_to_hide & ADJ_LOWER_SCREEN_EDGE)) {
            rectangles[count++] = (xcb_rectangle_t){
                .x = br.x,
                .y = con->rect.height + (br.height + br.y),
                .width = con->rect.width + br.width,
                .height = -(br.height + br.y),
            };
        }
        /* pixel border have an additional line at the top */
        if (border_style == BS_PIXEL && !(borders_to_hide & ADJ_UPPER_SCREEN_EDGE)) {
            rectangles[count++] = (xcb_rectangle_t){
                .x = br.x,
                .y = 0,
                .width = con->rect.width + br.width,
                .height = br.y,
            };
        }
    }

    return count;
}

static void set_hidden_state(GWMContainer* con)
{
    if (con->window == NULL) {
        return;
    }

    GWMContainerState* state = state_for_frame(con->frame.id);
    bool should_be_hidden = container_is_hidden(con);
    if (should_be_hidden == state->isHidden) {
        return;
    }

    if (should_be_hidden) {
        DEBUG("setting _NET_WM_STATE_HIDDEN for con = %p\n", con);
        xcb_gwm_add_property_atom(gConn, con->window->id, A__NET_WM_STATE, A__NET_WM_STATE_HIDDEN);
    } else {
        DEBUG("removing _NET_WM_STATE_HIDDEN for con = %p\n", con);
        xcb_gwm_remove_property_atom(gConn, con->window->id, A__NET_WM_STATE, A__NET_WM_STATE_HIDDEN);
    }

    state->isHidden = should_be_hidden;
}

static void x_shape_frame(GWMContainer* con, xcb_shape_sk_t shape_kind)
{
    g_assert(con->window);

    xcb_shape_combine(gConn, XCB_SHAPE_SO_SET, shape_kind, shape_kind, con->frame.id, con->windowRect.x + con->borderWidth, con->windowRect.y + con->borderWidth, con->window->id);
    xcb_rectangle_t rectangles[4];
    size_t rectangles_count = x_get_border_rectangles(con, rectangles);
    if (rectangles_count) {
        xcb_shape_rectangles(gConn, XCB_SHAPE_SO_UNION, shape_kind, XCB_CLIP_ORDERING_UNSORTED, con->frame.id, 0, 0, rectangles_count, rectangles);
    }
}

static void x_unshape_frame(GWMContainer* con, xcb_shape_sk_t shape_kind)
{
    g_assert(con->window);

    xcb_shape_mask(gConn, XCB_SHAPE_SO_SET, shape_kind, con->frame.id, 0, 0, XCB_PIXMAP_NONE);
}

static void set_shape_state(GWMContainer* con, bool need_reshape)
{
    if (!gShapeSupported || con->window == NULL) {
        return;
    }

    GWMContainerState *state;
    if ((state = state_for_frame(con->frame.id)) == NULL) {
        ERROR("window state for con %p not found\n", con);
        return;
    }

    if (need_reshape && container_is_floating(con)) {
        if (con->window->shaped) {
            x_shape_frame(con, XCB_SHAPE_SK_BOUNDING);
        }
        if (con->window->inputShaped) {
            x_shape_frame(con, XCB_SHAPE_SK_INPUT);
        }
    }

    if (state->wasFloating && !container_is_floating(con)) {
        if (con->window->shaped) {
            x_unshape_frame(con, XCB_SHAPE_SK_BOUNDING);
        }
        if (con->window->inputShaped) {
            x_unshape_frame(con, XCB_SHAPE_SK_INPUT);
        }
    }
}

static void x_push_node_unmaps(GWMContainer* con)
{
    GWMContainer* current;
    GWMContainerState* state;

    state = state_for_frame(con->frame.id);

    if (state->unmapNow) {
        xcb_void_cookie_t cookie;
        if (con->window != NULL) {
            long data[] = {XCB_ICCCM_WM_STATE_WITHDRAWN, XCB_NONE};
            xcb_change_property(gConn, XCB_PROP_MODE_REPLACE, con->window->id, A_WM_STATE, A_WM_STATE, 32, 2, data);
        }

        cookie = xcb_unmap_window(gConn, con->frame.id);
        DEBUG("unmapping container %p / %s (serial %d)\n", con, con->name, cookie.sequence);
        if (con->window != NULL) {
            con->ignoreUnmap++;
            DEBUG("ignore_unmap for con %p (frame 0x%08x) now %d\n", con, con->frame.id, con->ignoreUnmap);
        }
        state->mapped = con->mapped;
    }

    TAILQ_FOREACH (current, &(con->nodesHead), nodes) {
        x_push_node_unmaps(current);
    }

    TAILQ_FOREACH (current, &(con->floatingHead), floatingWindows) {
        x_push_node_unmaps(current);
    }
}

static bool is_con_attached(GWMContainer* con)
{
    if (con->parent == NULL) {
        return false;
    }

    GWMContainer *current;
    TAILQ_FOREACH (current, &(con->parent->nodesHead), nodes) {
        if (current == con) {
            return true;
        }
    }

    return false;
}
