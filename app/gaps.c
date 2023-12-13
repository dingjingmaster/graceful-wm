//
// Created by dingjing on 23-12-10.
//

#include "gaps.h"

#include "val.h"
#include "log.h"
#include "container.h"
#include "output.h"
#include "resize.h"

GWMGaps gaps_for_workspace(GWMContainer* con)
{
    GWMContainer* workspace = container_get_workspace(con);
    if (workspace == NULL) {
        return (GWMGaps) {0, 0, 0, 0, 0};
    }

    bool one_child = container_num_visible_children(workspace) <= 1
            || (container_num_children(workspace) == 1 && (TAILQ_FIRST(&(workspace->nodesHead))->layout == L_TABBED
            || TAILQ_FIRST(&(workspace->nodesHead))->layout == L_STACKED));

    if (one_child) {
        return (GWMGaps) {0, 0, 0, 0, 0};
    }

    GWMGaps gaps = {
        .inner = (workspace->gaps.inner),
        .top = 1,
        .right = 1,
        .bottom = 1,
        .left = 1};

    return gaps;
}

void gaps_reapply_workspace_assignments(void)
{
    GWMContainer* output, *workspace = NULL;
    TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
        GWMContainer* content = output_get_content(output);
        TAILQ_FOREACH (workspace, &(content->nodesHead), nodes) {
            DEBUG("updating gap assignments for workspace %s", workspace->name);
            workspace->gaps = gaps_for_workspace(workspace);
        }
    }
}

GWMGaps gaps_calculate_effective_gaps(GWMContainer *con)
{
    GWMContainer* workspace = container_get_workspace(con);
    if (workspace == NULL) {
        return (GWMGaps) {0, 0, 0, 0, 0};
    }

    bool one_child = container_num_visible_children(workspace) <= 1
        || (container_num_children(workspace) == 1
        && (TAILQ_FIRST(&(workspace->nodesHead))->layout == L_TABBED || TAILQ_FIRST(&(workspace->nodesHead))->layout == L_STACKED));

    if (one_child) {
        return (GWMGaps) {2, 2, 2, 2, 2};
    }

    GWMGaps gaps = {
        .inner = (workspace->gaps.inner + 1),
        .top = 0,
        .right = 0,
        .bottom = 0,
        .left = 0};

    if (one_child) {
        gaps.top = workspace->gaps.top + 1;
        gaps.right = workspace->gaps.right + 1;
        gaps.bottom = workspace->gaps.bottom + 1;
        gaps.left = workspace->gaps.left + 1;
    }

    return gaps;
}

bool gaps_should_inset_con(GWMContainer *con, int children)
{
    if (con->parent == NULL) {
        return false;
    }

    if (container_inside_floating(con)) {
        return false;
    }

    const bool leafOrStackedTabbed = container_is_leaf(con) || (con->layout == L_STACKED || con->layout == L_TABBED);
    if (leafOrStackedTabbed && con->parent->type == CT_WORKSPACE) {
        return true;
    }

    if (leafOrStackedTabbed
        && !container_inside_stacked_or_tabbed(con)
        && con->parent->type == CT_CON
        && (con->parent->layout == L_SPLIT_H || con->parent->layout == L_SPLIT_V)) {
        return true;
    }

    return false;
}

bool gaps_has_adjacent_container(GWMContainer *con, GWMDirection direction)
{
    GWMContainer* workspace = container_get_workspace(con);
    GWMContainer* fullscreen = container_get_full_screen_con(workspace, CF_GLOBAL);
    if (fullscreen == NULL) {
        fullscreen = container_get_full_screen_con(workspace, CF_OUTPUT);
    }

    if (con == fullscreen) {
        return false;
    }

    GWMContainer* first = con;
    GWMContainer* second = NULL;
    bool found_neighbor = resize_find_tiling_participants(&first, &second, direction, false);
    if (!found_neighbor) {
        return false;
    }

    if (fullscreen == NULL) {
        return true;
    }

    return container_has_parent(con, fullscreen) && container_has_parent(second, fullscreen);
}
