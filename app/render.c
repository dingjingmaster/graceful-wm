//
// Created by dingjing on 23-11-29.
//

#include "render.h"

#include <math.h>
#include <assert.h>

#include "x.h"
#include "log.h"
#include "val.h"
#include "gaps.h"
#include "utils.h"
#include "types.h"
#include "output.h"
#include "container.h"


static void render_output(GWMContainer* con);
static void render_root(GWMContainer* con, GWMContainer* fullscreen);
static int* precalculate_sizes(GWMContainer* con, GWMRenderParams* p);
static void render_con_dock_area(GWMContainer* con, GWMContainer* child, GWMRenderParams* p);
static void render_con_split(GWMContainer* con, GWMContainer* child, GWMRenderParams* p, int i);
static void render_con_tabbed(GWMContainer* con, GWMContainer* child, GWMRenderParams* p, int i);
static void render_con_stacked(GWMContainer* con, GWMContainer* child, GWMRenderParams* p, int i);


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
        GWMGaps gaps = gaps_calculate_effective_gaps(con);
        GWMRect inset = (GWMRect){
            gaps.left,
            gaps.top,
            -(gaps.left + gaps.right),
            -(gaps.top + gaps.bottom),
        };
        con->rect = util_rect_add(con->rect, inset);
        params.rect = util_rect_add(params.rect, inset);
        params.x += gaps.left;
        params.y += gaps.top;
    }

    if (gaps_should_inset_con(con, params.children)) {
        GWMGaps gaps = gaps_calculate_effective_gaps(con);
        GWMRect inset = (GWMRect){
            gaps_has_adjacent_container(con, D_LEFT) ? gaps.inner / 2 : gaps.inner,
            gaps_has_adjacent_container(con, D_UP) ? gaps.inner / 2 : gaps.inner,
            gaps_has_adjacent_container(con, D_RIGHT) ? -(gaps.inner / 2) : -gaps.inner,
            gaps_has_adjacent_container(con, D_DOWN) ? -(gaps.inner / 2) : -gaps.inner,
        };
        inset.width -= inset.x;
        inset.height -= inset.y;

        if (con->fullScreenMode == CF_NONE) {
            params.rect = util_rect_add(params.rect, inset);
            con->rect = util_rect_add(con->rect, inset);
        }
        inset.height = 0;

        params.x = con->rect.x;
        params.y = con->rect.y;
    }

    int i = 0;
    con->mapped = true;

    if (con->window) {
        GWMRect inset = (GWMRect){
            .x = 0,
            .y = 0,
            .width = con->rect.width,
            .height = con->rect.height,
        };
        if (con->fullScreenMode == CF_NONE) {
            DEBUG("deco_rect.height = %d", con->decorationRect.height);
            GWMRect bsr = container_border_style_rect(con);
            DEBUG("bsr at %dx%d with size %dx%d", bsr.x, bsr.y, bsr.width, bsr.height);
            inset = util_rect_add(inset, bsr);
        }

        inset.width -= (2 * con->borderWidth);
        inset.height -= (2 * con->borderWidth);

        inset = util_rect_sanitize_dimensions(inset);
        con->windowRect = inset;

        DEBUG("child will be at %dx%d with size %dx%d", inset.x, inset.y, inset.width, inset.height);
    }

    /* Check for fullscreen nodes */
    GWMContainer* fullscreen = NULL;
    if (con->type != CT_OUTPUT) {
        fullscreen = container_get_full_screen_con(con, (con->type == CT_ROOT ? CF_GLOBAL : CF_OUTPUT));
    }
    if (fullscreen) {
        fullscreen->rect = params.rect;
        x_raise_container(fullscreen);
        render_container(fullscreen);
        if (con->type != CT_ROOT) {
            return;
        }
    }

    params.decoHeight = render_deco_height();

    params.sizes = precalculate_sizes(con, &params);

    if (con->layout == L_OUTPUT) {
        if (container_is_internal(con)) {
            goto free_params;
        }
        render_output(con);
    }
    else if (con->type == CT_ROOT) {
        render_root(con, fullscreen);
    }
    else {
        GWMContainer* child;
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            assert(params.children > 0);

            if (con->layout == L_SPLIT_H || con->layout == L_SPLIT_V) {
                render_con_split(con, child, &params, i);
            }
            else if (con->layout == L_STACKED) {
                render_con_stacked(con, child, &params, i);
            }
            else if (con->layout == L_TABBED) {
                render_con_tabbed(con, child, &params, i);
            }
            else if (con->layout == L_DOCK_AREA) {
                render_con_dock_area(con, child, &params);
            }

            child->rect = util_rect_sanitize_dimensions(child->rect);

            DEBUG("child at (%d, %d) with (%d x %d)", child->rect.x, child->rect.y, child->rect.width, child->rect.height);
            x_raise_container(child);
            render_container(child);

            if (con->layout == L_SPLIT_H || con->layout == L_SPLIT_V) {
                if (container_is_leaf(child)) {
                    if (child->borderStyle == BS_NORMAL) {
                        child->decorationRect.width = child->rect.width;
                    }
                }
            }

            i++;
        }

        if (con->layout == L_STACKED || con->layout == L_TABBED) {
            TAILQ_FOREACH_REVERSE (child, &(con->focusHead), focusHead, focused) {
                x_raise_container(child);
            }
            if ((child = TAILQ_FIRST(&(con->focusHead)))) {
                render_container(child);
            }

            if (params.children != 1)
                x_raise_container(con);
        }
    }

free_params:
    FREE(params.sizes);
}

int render_deco_height(void)
{
    int deco_height = 6;
//    if (config.font.height & 0x01)
//        ++deco_height;
    return deco_height;
}


static int *precalculate_sizes(GWMContainer* con, GWMRenderParams* p)
{
    if ((con->layout != L_SPLIT_H && con->layout != L_SPLIT_V) || p->children <= 0) {
        return NULL;
    }

    int *sizes = g_malloc0(p->children * sizeof(int));
    assert(!TAILQ_EMPTY(&con->nodesHead));

    GWMContainer* child;
    int i = 0, assigned = 0;
    int total = container_rect_size_in_orientation(con);
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        double percentage = child->percent > 0.0 ? child->percent : 1.0 / p->children;
        assigned += sizes[i++] = lround(percentage * total);
    }
    assert(assigned == total ||
           (assigned > total && assigned - total <= p->children * 2) ||
           (assigned < total && total - assigned <= p->children * 2));
    int signal = assigned < total ? 1 : -1;
    while (assigned != total) {
        for (i = 0; i < p->children && assigned != total; ++i) {
            sizes[i] += signal;
            assigned += signal;
        }
    }

    return sizes;
}

static void render_root(GWMContainer* con, GWMContainer* fullscreen)
{
    GWMContainer* output;
    if (!fullscreen) {
        TAILQ_FOREACH (output, &(con->nodesHead), nodes) {
            render_container(output);
        }
    }

    DEBUG("Rendering floating windows:");
    TAILQ_FOREACH (output, &(con->nodesHead), nodes) {
        if (container_is_internal(output)) {
            continue;
        }
        GWMContainer* content = output_get_content(output);
        if (!content || TAILQ_EMPTY(&(content->focusHead))) {
            DEBUG("Skipping this output because it is currently being destroyed.");
            continue;
        }
        GWMContainer* workspace = TAILQ_FIRST(&(content->focusHead));
        GWMContainer* fullscreen = container_get_full_screen_covering_ws(workspace);
        GWMContainer* child;
        TAILQ_FOREACH (child, &(workspace->floatingHead), floatingWindows) {
            if (fullscreen != NULL) {
//                if (config.popup_during_fullscreen != PDF_SMART || fullscreen->window == NULL) {
//                    continue;
//                }

                GWMContainer* floating_child = container_descend_focused(child);
                if (container_find_transient_for_window(floating_child, fullscreen->window->id)) {
                    DEBUG("Rendering floating child even though in fullscreen mode: floating->transient_for (0x%08x) --> fullscreen->id (0x%08x)",
                            floating_child->window->transientFor, fullscreen->window->id);
                }
                else {
                    continue;
                }
            }
            DEBUG("floating child at (%d,%d) with %d x %d", child->rect.x, child->rect.y, child->rect.width, child->rect.height);
            x_raise_container(child);
            render_container(child);
        }
    }
}

static void render_output(GWMContainer* con)
{
    GWMContainer* child, *dockchild;

    int x = con->rect.x;
    int y = con->rect.y;
    int height = con->rect.height;

    GWMContainer* content = NULL;
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        if (child->type == CT_CON) {
            if (content != NULL) {
                DEBUG("More than one CT_CON on output container");
                assert(false);
            }
            content = child;
        }
        else if (child->type != CT_DOCK_AREA) {
            DEBUG("Child %p of type %d is inside the OUTPUT con", child, child->type);
            assert(false);
        }
    }

    if (content == NULL) {
        DEBUG("Skipping this output because it is currently being destroyed.");
        return;
    }

    GWMContainer* ws = container_get_full_screen_con(content, CF_OUTPUT);
    if (!ws) {
        DEBUG("Skipping this output because it is currently being destroyed.");
        return;
    }
    GWMContainer* fullscreen = container_get_full_screen_con(ws, CF_OUTPUT);
    if (fullscreen) {
        fullscreen->rect = con->rect;
        x_raise_container(fullscreen);
        render_container(fullscreen);
        return;
    }

    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        if (child->type != CT_DOCK_AREA) {
            continue;
        }

        child->rect.height = 0;
        TAILQ_FOREACH (dockchild, &(child->nodesHead), nodes) {
            child->rect.height += dockchild->geoRect.height;
        }

        height -= child->rect.height;
    }

    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        if (child->type == CT_CON) {
            child->rect.x = x;
            child->rect.y = y;
            child->rect.width = con->rect.width;
            child->rect.height = height;
        }
        child->rect.x = x;
        child->rect.y = y;
        child->rect.width = con->rect.width;

        child->decorationRect.x = 0;
        child->decorationRect.y = 0;
        child->decorationRect.width = 0;
        child->decorationRect.height = 0;

        y += child->rect.height;

        DEBUG("child at (%d, %d) with (%d x %d)", child->rect.x, child->rect.y, child->rect.width, child->rect.height);
        x_raise_container(child);
        render_container(child);
    }
}

static void render_con_split(GWMContainer* con, GWMContainer* child, GWMRenderParams* p, int i)
{
    assert(con->layout == L_SPLIT_H || con->layout == L_SPLIT_V);

    if (con->layout == L_SPLIT_H) {
        child->rect.x = p->x;
        child->rect.y = p->y;
        child->rect.width = p->sizes[i];
        child->rect.height = p->rect.height;
        p->x += child->rect.width;
    }
    else {
        child->rect.x = p->x;
        child->rect.y = p->y;
        child->rect.width = p->rect.width;
        child->rect.height = p->sizes[i];
        p->y += child->rect.height;
    }

    if (container_is_leaf(child)) {
        if (child->borderStyle == BS_NORMAL) {
            child->decorationRect.x = 0;
            child->decorationRect.y = 0;
            child->decorationRect.width = child->rect.width;
            child->decorationRect.height = p->decoHeight;
        }
        else {
            child->decorationRect.x = 0;
            child->decorationRect.y = 0;
            child->decorationRect.width = 0;
            child->decorationRect.height = 0;
        }
    }
}

static void render_con_stacked(GWMContainer* con, GWMContainer* child, GWMRenderParams* p, int i)
{
    assert(con->layout == L_STACKED);

    child->rect.x = p->x;
    child->rect.y = p->y;
    child->rect.width = p->rect.width;
    child->rect.height = p->rect.height;

    child->decorationRect.x = p->x - con->rect.x;
    child->decorationRect.y = p->y - con->rect.y + (i * p->decoHeight);
    child->decorationRect.width = child->rect.width;
    child->decorationRect.height = p->decoHeight;

    if (p->children > 1 || (child->borderStyle != BS_PIXEL && child->borderStyle != BS_NONE)) {
        child->rect.y += (p->decoHeight * p->children);
        child->rect.height -= (p->decoHeight * p->children);
    }
}

static void render_con_tabbed(GWMContainer* con, GWMContainer* child, GWMRenderParams* p, int i)
{
    assert(con->layout == L_TABBED);

    child->rect.x = p->x;
    child->rect.y = p->y;
    child->rect.width = p->rect.width;
    child->rect.height = p->rect.height;

    child->decorationRect.width = floor((float)child->rect.width / p->children);
    child->decorationRect.x = p->x - con->rect.x + i * child->decorationRect.width;
    child->decorationRect.y = p->y - con->rect.y;

    if (i == (p->children - 1)) {
        child->decorationRect.width = child->rect.width - child->decorationRect.x;
    }

    if (p->children > 1 || (child->borderStyle != BS_PIXEL && child->borderStyle != BS_NONE)) {
        child->rect.y += p->decoHeight;
        child->rect.height -= p->decoHeight;
        child->decorationRect.height = p->decoHeight;
    }
    else {
        child->decorationRect.height = (child->borderStyle == BS_PIXEL ? 1 : 0);
    }
}

static void render_con_dock_area(GWMContainer* con, GWMContainer* child, GWMRenderParams* p)
{
    assert(con->layout == L_DOCK_AREA);

    child->rect.x = p->x;
    child->rect.y = p->y;
    child->rect.width = p->rect.width;
    child->rect.height = child->geoRect.height;

    child->decorationRect.x = 0;
    child->decorationRect.y = 0;
    child->decorationRect.width = 0;
    child->decorationRect.height = 0;
    p->y += child->rect.height;
}
