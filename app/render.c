//
// Created by dingjing on 23-11-29.
//

#include "render.h"

#include "log.h"
#include "val.h"
#include "utils.h"
#include "types.h"
#include "container.h"

void render_container(GWMContainer *con)
{
    DEBUG("render container start ...");

    GWMRenderParams params = {
        .rect = con->rect,
        .x = con->rect.x,
        .y = con->rect.y,
        .children = container_num_children(con),
    };

    DEBUG(_("Rendering node %p / %s / layout %d / children %d"), con, con->name, con->layout, params.children);
    if (con->type == CT_WORKSPACE) {
//        GWMGaps gaps = calculate_effective_gaps(con);
//        GWMRect inset = (GWMRect){
//            gaps.left,
//            gaps.top,
//            -(gaps.left + gaps.right),
//            -(gaps.top + gaps.bottom),
//        };
//        con->rect = rect_add(con->rect, inset);
//        params.rect = rect_add(params.rect, inset);
//        params.x += gaps.left;
//        params.y += gaps.top;
//    }
//
//    if (gaps_should_inset_con(con, params.children)) {
//        gaps_t gaps = calculate_effective_gaps(con);
//        Rect inset = (Rect){
//            gaps_has_adjacent_container(con, D_LEFT) ? gaps.inner / 2 : gaps.inner,
//            gaps_has_adjacent_container(con, D_UP) ? gaps.inner / 2 : gaps.inner,
//            gaps_has_adjacent_container(con, D_RIGHT) ? -(gaps.inner / 2) : -gaps.inner,
//            gaps_has_adjacent_container(con, D_DOWN) ? -(gaps.inner / 2) : -gaps.inner,
//        };
//        inset.width -= inset.x;
//        inset.height -= inset.y;
//
//        if (con->fullscreen_mode == CF_NONE) {
//            params.rect = rect_add(params.rect, inset);
//            con->rect = rect_add(con->rect, inset);
//        }
//        inset.height = 0;
//
//        params.x = con->rect.x;
//        params.y = con->rect.y;
//    }
//
//    int i = 0;
//    con->mapped = true;
//
//    /* if this container contains a window, set the coordinates */
//    if (con->window) {
//        /* depending on the border style, the rect of the child window
//         * needs to be smaller */
//        Rect inset = (Rect){
//            .x = 0,
//            .y = 0,
//            .width = con->rect.width,
//            .height = con->rect.height,
//        };
//        if (con->fullscreen_mode == CF_NONE) {
//            DLOG("deco_rect.height = %d\n", con->deco_rect.height);
//            Rect bsr = con_border_style_rect(con);
//            DLOG("bsr at %dx%d with size %dx%d\n",
//                 bsr.x, bsr.y, bsr.width, bsr.height);
//
//            inset = rect_add(inset, bsr);
//        }
//
//        /* Obey x11 border */
//        inset.width -= (2 * con->border_width);
//        inset.height -= (2 * con->border_width);
//
//        inset = rect_sanitize_dimensions(inset);
//        con->window_rect = inset;
//
//        /* NB: We used to respect resize increment size hints for tiling
//         * windows up until commit 0db93d9 here. However, since all terminal
//         * emulators cope with ignoring the size hints in a better way than we
//         * can (by providing their fake-transparency or background color), this
//         * code was removed. See also https://bugs.i3wm.org/540 */
//
//        DLOG("child will be at %dx%d with size %dx%d\n",
//             inset.x, inset.y, inset.width, inset.height);
//    }
//
//    /* Check for fullscreen nodes */
//    Con *fullscreen = NULL;
//    if (con->type != CT_OUTPUT) {
//        fullscreen = con_get_fullscreen_con(con, (con->type == CT_ROOT ? CF_GLOBAL : CF_OUTPUT));
//    }
//    if (fullscreen) {
//        fullscreen->rect = params.rect;
//        x_raise_con(fullscreen);
//        render_con(fullscreen);
//        /* Fullscreen containers are either global (underneath the CT_ROOT
//         * container) or per-output (underneath the CT_CONTENT container). For
//         * global fullscreen containers, we cannot abort rendering here yet,
//         * because the floating windows (with popup_during_fullscreen smart)
//         * have not yet been rendered (see the CT_ROOT code path below). See
//         * also https://bugs.i3wm.org/1393 */
//        if (con->type != CT_ROOT) {
//            return;
//        }
//    }
//
//    /* find the height for the decorations */
//    params.deco_height = render_deco_height();
//
//    /* precalculate the sizes to be able to correct rounding errors */
//    params.sizes = precalculate_sizes(con, &params);
//
//    if (con->layout == L_OUTPUT) {
//        /* Skip i3-internal outputs */
//        if (con_is_internal(con))
//            goto free_params;
//        render_output(con);
//    } else if (con->type == CT_ROOT) {
//        render_root(con, fullscreen);
//    } else {
//        Con *child;
//        TAILQ_FOREACH (child, &(con->nodes_head), nodes) {
//            assert(params.children > 0);
//
//            if (con->layout == L_SPLITH || con->layout == L_SPLITV) {
//                render_con_split(con, child, &params, i);
//            } else if (con->layout == L_STACKED) {
//                render_con_stacked(con, child, &params, i);
//            } else if (con->layout == L_TABBED) {
//                render_con_tabbed(con, child, &params, i);
//            } else if (con->layout == L_DOCKAREA) {
//                render_con_dockarea(con, child, &params);
//            }
//
//            child->rect = rect_sanitize_dimensions(child->rect);
//
//            DLOG("child at (%d, %d) with (%d x %d)\n",
//                 child->rect.x, child->rect.y, child->rect.width, child->rect.height);
//            x_raise_con(child);
//            render_con(child);
//
//            /* render_con_split() sets the deco_rect width based on the rect
//             * width, but the render_con() call updates the rect width by
//             * applying gaps, so we need to update deco_rect. */
//            if (con->layout == L_SPLITH || con->layout == L_SPLITV) {
//                if (con_is_leaf(child)) {
//                    if (child->border_style == BS_NORMAL) {
//                        child->deco_rect.width = child->rect.width;
//                    }
//                }
//            }
//
//            i++;
//        }
//
//        /* in a stacking or tabbed container, we ensure the focused client is raised */
//        if (con->layout == L_STACKED || con->layout == L_TABBED) {
//            TAILQ_FOREACH_REVERSE (child, &(con->focus_head), focus_head, focused) {
//                x_raise_con(child);
//            }
//            if ((child = TAILQ_FIRST(&(con->focus_head)))) {
//                /* By rendering the stacked container again, we handle the case
//                 * that we have a non-leaf-container inside the stack. In that
//                 * case, the children of the non-leaf-container need to be
//                 * raised as well. */
//                render_con(child);
//            }
//
//            if (params.children != 1)
//                /* Raise the stack con itself. This will put the stack
//                 * decoration on top of every stack window. That way, when a
//                 * new window is opened in the stack, the old window will not
//                 * obscure part of the decoration (it’s unmapped afterwards). */
//                x_raise_con(con);
//        }
    }

free_params:
    FREE(params.sizes);
}

int render_deco_height(void)
{
    return 0;
}
