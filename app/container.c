//
// Created by dingjing on 23-11-24.
//

#include <sys/time.h>
#include "container.h"

#include "x.h"
#include "val.h"
#include "log.h"
#include "tree.h"
#include "match.h"
#include "workspace.h"
#include "floating.h"
#include "xcb.h"
#include "extend-wm-hints.h"
#include "output.h"
#include "utils.h"


typedef struct bfs_entry bfs_entry;

struct bfs_entry
{
    GWMContainer*           con;
    TAILQ_ENTRY(bfs_entry)  entries;
};


static bool has_outer_gaps(GWMGaps gaps);
static int num_focus_heads(GWMContainer* con);
static void container_raise(GWMContainer* con);
static void container_on_remove_child(GWMContainer* con);
static GWMRect con_border_style_rect_without_title(GWMContainer* con);
static void container_set_fullscreen_mode(GWMContainer* con, GWMFullScreenMode fullscreenMode);
static void _container_attach(GWMContainer* con, GWMContainer* parent, GWMContainer* previous, bool ignoreFocus);
static bool _container_move_to_con(GWMContainer* con, GWMContainer* target, bool behindFocused, bool fixCoordinates, bool dontWarp, bool ignoreFocus, bool fixPercentage);


void container_activate(GWMContainer *con)
{
    container_focus(con);
    container_raise(con);
}

GWMContainer *container_descend_focused(GWMContainer *con)
{
    g_return_val_if_fail(con, NULL);

    GWMContainer* next = con;

    while (next != gFocused && !TAILQ_EMPTY(&(next->focusHead))) {
        next = TAILQ_FIRST(&(next->focusHead));
    }

    return next;
}

void container_free(GWMContainer *con)
{
    free(con->name);
    FREE(con->decorationRenderParams);
    TAILQ_REMOVE(&gAllContainer, con, allContainers);
    while (!TAILQ_EMPTY(&(con->swallowHead))) {
        GWMMatch *match = TAILQ_FIRST(&(con->swallowHead));
        TAILQ_REMOVE(&(con->swallowHead), match, matches);
        match_free(match);
        free(match);
    }
    while (!TAILQ_EMPTY(&(con->marksHead))) {
        GWMMark* mark = TAILQ_FIRST(&(con->marksHead));
        TAILQ_REMOVE(&(con->marksHead), mark, marks);
        FREE(mark->name);
        FREE(mark);
    }

    DEBUG(_("con %p freed"), con);

    free(con);
}

void container_focus(GWMContainer *con)
{
    g_assert(con != NULL);
    DEBUG(_("container_focus = %p"), con);

    /* 1: set focused-pointer to the new con */
    /* 2: exchange the position of the container in focus stack of the parent all the way up */
    TAILQ_REMOVE(&(con->parent->focusHead), con, focused);
    TAILQ_INSERT_HEAD(&(con->parent->focusHead), con, focused);
    if (con->parent->parent != NULL) {
        container_focus(con->parent);
    }

    gFocused = con;
    /* We can't blindly reset non-leaf containers since they might have
     * other urgent children. Therefore we only reset leafs and propagate
     * the changes upwards via container_update_parents_urgency() which does proper
     * checks before resetting the urgency.
     */
    if (con->urgent && container_is_leaf(con)) {
        container_set_urgency(con, false);
        container_update_parents_urgency(con);
        workspace_update_urgent_flag(container_get_workspace(con));
//        ipc_send_window_event("urgent", con);
    }
}

bool container_exists(GWMContainer *con)
{
    return container_by_container_id((long)con) != NULL;
}

void container_detach(GWMContainer *con)
{
    container_force_split_parents_redraw(con);
    if (con->type == CT_FLOATING_CON) {
        TAILQ_REMOVE(&(con->parent->floatingHead), con, floatingWindows);
        TAILQ_REMOVE(&(con->parent->focusHead), con, focused);
    }
    else {
        TAILQ_REMOVE(&(con->parent->nodesHead), con, nodes);
        TAILQ_REMOVE(&(con->parent->focusHead), con, focused);
    }
}

bool container_is_leaf(GWMContainer *con)
{
    return TAILQ_EMPTY(&(con->nodesHead));
}

bool container_is_split(GWMContainer *con)
{
    if (container_is_leaf(con)) {
        return false;
    }

    switch (con->layout) {
        case L_DOCK_AREA:
        case L_OUTPUT: {
            return false;
        }
        default: {
            return true;
        }
    }
}

bool container_is_sticky(GWMContainer *con)
{
    if (con->sticky) {
        return true;
    }

    GWMContainer* child;
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        if (container_is_sticky(child)) {
            return true;
        }
    }

    return false;
}

bool container_is_docked(GWMContainer *con)
{
    if (con->parent == NULL) {
        return false;
    }

    if (con->parent->type == CT_DOCK_AREA) {
        return true;
    }

    return container_is_docked(con->parent);
}

int container_num_windows(GWMContainer *con)
{
    g_return_val_if_fail(con, 0);

    if (container_has_managed_window(con)) {
        return 1;
    }

    int num = 0;
    GWMContainer* current = NULL;
    TAILQ_FOREACH (current, &(con->nodesHead), nodes) {
        num += container_num_windows(current);
    }

    TAILQ_FOREACH (current, &(con->floatingHead), floatingWindows) {
        num += container_num_windows(current);
    }

    return num;
}

int container_border_style(GWMContainer *con)
{
    if (con->fullScreenMode == CF_OUTPUT || con->fullScreenMode == CF_GLOBAL) {
        DEBUG(_("this one is fullscreen! overriding BS_NONE"));
        return BS_NONE;
    }

    if (con->parent != NULL) {
        if (con->parent->layout == L_STACKED) {
            return (container_num_children(con->parent) == 1 ? con->borderStyle : BS_NORMAL);
        }

        if (con->parent->layout == L_TABBED && con->borderStyle != BS_NORMAL) {
            return (container_num_children(con->parent) == 1 ? con->borderStyle : BS_NORMAL);
        }

        if (con->parent->type == CT_DOCK_AREA) {
            return BS_NONE;
        }
    }

    return con->borderStyle;
}

void container_fix_percent(GWMContainer *con)
{
    GWMContainer* child;
    int children = container_num_children(con);

    double total = 0.0;
    int children_with_percent = 0;
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        if (child->percent > 0.0) {
            total += child->percent;
            ++children_with_percent;
        }
    }

    /* if there were children without a percentage set, set to a value that
     * will make those children proportional to all others */
    if (children_with_percent != children) {
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            if (child->percent <= 0.0) {
                if (children_with_percent == 0) {
                    total += (child->percent = 1.0);
                }
                else {
                    total += (child->percent = total / children_with_percent);
                }
            }
        }
    }

    if (total == 0.0) {
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            child->percent = 1.0 / children;
        }
    }
    else if (total != 1.0) {
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            child->percent /= total;
        }
    }
}

bool container_is_internal(GWMContainer *con)
{
    return (con->name[0] == '_' && con->name[1] == '_');
}

bool container_is_floating(GWMContainer *con)
{
    g_assert(con != NULL);
    return (con->floating >= FLOATING_AUTO_ON);
}

int container_num_children(GWMContainer *con)
{
    GWMContainer* child;
    int children = 0;
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        children++;
    }

    return children;
}

bool container_has_children(GWMContainer *con)
{
    return (!container_is_leaf(con) || !TAILQ_EMPTY(&(con->floatingHead)));
}

bool container_inside_focused(GWMContainer *con)
{
    if (con == gFocused) {
        return true;
    }

    if (!con->parent) {
        return false;
    }

    return container_inside_focused(con->parent);
}

bool container_accepts_window(GWMContainer *con)
{
    if (con->type == CT_WORKSPACE)
        return false;

    if (container_is_split(con)) {
        DEBUG(_("container %p does not accept windows, it is a split container."), con);
        return false;
    }

    return (con->window == NULL);
}

GWMContainer *container_by_mark(const char *mark)
{
    GWMContainer* con;
    TAILQ_FOREACH (con, &gAllContainer, allContainers) {
        if (container_has_mark(con, mark)) {
            return con;
        }
    }

    return NULL;
}

bool container_has_urgent_child(GWMContainer *con)
{
    GWMContainer* child;

    if (container_is_leaf(con)) {
        return con->urgent;
    }

    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        if (container_has_urgent_child(child)) {
            return true;
        }
    }

    return false;
}

void container_activate_unblock(GWMContainer *con)
{
    GWMContainer* ws = container_get_workspace(con);
    GWMContainer* previous_focus = gFocused;
    GWMContainer* fullscreen_on_ws = container_get_full_screen_covering_ws(ws);

    if (fullscreen_on_ws && fullscreen_on_ws != con && !container_has_parent(con, fullscreen_on_ws)) {
        container_disable_full_screen(fullscreen_on_ws);
    }

    container_activate(con);

    if (ws != container_get_workspace(previous_focus)) {
        container_activate(previous_focus);
        workspace_show(ws);
        container_activate(con);
    }
}

GWMRect container_minimum_size(GWMContainer *con)
{
    DEBUG("Determining minimum size for con %p", con);

    if (container_is_leaf(con)) {
        DEBUG("leaf node, returning 75x50");
        return (GWMRect){0, 0, 75, 50};
    }

    if (con->type == CT_FLOATING_CON) {
        DEBUG("floating con");
        GWMContainer* child = TAILQ_FIRST(&(con->nodesHead));
        return container_minimum_size(child);
    }

    if (con->layout == L_STACKED || con->layout == L_TABBED) {
        uint32_t max_width = 0, max_height = 0, deco_height = 0;
        GWMContainer* child;
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            GWMRect min = container_minimum_size(child);
            deco_height += child->decorationRect.height;
            max_width = MAX(max_width, min.width);
            max_height = MAX(max_height, min.height);
        }
        DEBUG("stacked/tabbed now, returning %d x %d + deco_rect = %d", max_width, max_height, deco_height);

        return (GWMRect){0, 0, max_width, max_height + deco_height};
    }

    if (container_is_split(con)) {
        uint32_t width = 0, height = 0;
        GWMContainer* child;
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            GWMRect min = container_minimum_size(child);
            if (con->layout == L_SPLIT_H) {
                width += min.width;
                height = MAX(height, min.height);
            }
            else {
                height += min.height;
                width = MAX(width, min.width);
            }
        }
        DEBUG("split container, returning width = %d x height = %d", width, height);

        return (GWMRect){0, 0, width, height};
    }

    ERROR("Unhandled case, type = %d, layout = %d, split = %d", con->type, con->layout, container_is_split(con));

    g_assert(false);
}

GWMContainer *container_by_container_id(long target)
{
    GWMContainer* con;
    TAILQ_FOREACH (con, &gAllContainer, allContainers) {
        if (con == (GWMContainer*)target) {
            return con;
        }
    }

    return NULL;
}

bool container_has_managed_window(GWMContainer *con)
{
    return (con != NULL && con->window != NULL && con->window->id != XCB_WINDOW_NONE && container_get_workspace(con) != NULL);
}

void container_disable_full_screen(GWMContainer *con)
{
    if (con->type == CT_WORKSPACE) {
        DEBUG(_("You cannot make a workspace fullscreen."));
        return;
    }

    DEBUG(_("disabling fullscreen for %p / %s"), con, con->name);

    if (con->fullScreenMode == CF_NONE) {
        DEBUG(_("fullscreen already disabled for %p / %s"), con, con->name);
        return;
    }

    container_set_fullscreen_mode(con, CF_NONE);
}

int container_num_visible_children(GWMContainer *con)
{
    g_return_val_if_fail(con, 0);

    int children = 0;
    GWMContainer* current = NULL;
    TAILQ_FOREACH (current, &(con->nodesHead), nodes) {
        if (!container_is_hidden(current) && container_is_leaf(current)) {
            children++;
        }
        else {
            children += container_num_visible_children(current);
        }
    }

    return children;
}

char* container_parse_title_format(GWMContainer *con)
{
    return "gwm-title";
#if 0
    g_assert(con->titleFormat != NULL);

    GWMWindow *win = con->window;

    /* We need to ensure that we only escape the window title if pango
     * is used by the current font. */
    const bool pango_markup = font_is_pango();

    char *title;
    char *class;
    char *instance;
    char *machine;
    if (win == NULL) {
        title = pango_escape_markup(container_get_tree_representation(con));
        class = g_strdup("gwm-frame");
        instance = g_strdup("gwm-frame");
        machine = g_strdup("");
    } else {
        title = pango_escape_markup(sstrdup((win->name == NULL) ? "" : (win->name)));
        class = pango_escape_markup(sstrdup((win->classClass == NULL) ? "" : win->classClass));
        instance = pango_escape_markup(sstrdup((win->classInstance == NULL) ? "" : win->classInstance));
        machine = pango_escape_markup(sstrdup((win->machine == NULL) ? "" : win->machine));
    }

    placeholder_t placeholders[] = {
        {.name = "%title", .value = title},
        {.name = "%class", .value = class},
        {.name = "%instance", .value = instance},
        {.name = "%machine", .value = machine},
    };
    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);

    char *formatted_str = format_placeholders(con->title_format, &placeholders[0], num);
    i3String *formatted = i3string_from_utf8(formatted_str);
    i3string_set_markup(formatted, pango_markup);

    free(formatted_str);
    free(title);
    free(class);
    free(instance);

    return formatted;
#endif
}

GWMContainer *container_get_output(GWMContainer *con)
{
    GWMContainer* result = con;
    while (result != NULL && result->type != CT_OUTPUT)
        result = result->parent;
    g_assert(result != NULL);
    return result;
}

GWMRect container_border_style_rect(GWMContainer *con)
{
    GWMRect result = con_border_style_rect_without_title(con);
    if (container_border_style(con) == BS_NORMAL && container_draw_decoration_into_frame(con)) {
        const int deco_height = render_deco_height();
        result.y += deco_height;
        result.height -= deco_height;
    }
    return result;
}

GWMContainer *container_by_frame_id(xcb_window_t frame)
{
    GWMContainer* con;
    TAILQ_FOREACH (con, &gAllContainer, allContainers) {
        if (con->frame.id == frame) {
            return con;
        }
    }
}

GWMOrientation container_orientation(GWMContainer *con)
{
    switch (con->layout) {
        case L_SPLIT_V:
            /* stacking containers behave like they are in vertical orientation */
        case L_STACKED:
            return VERT;

        case L_SPLIT_H:
            /* tabbed containers behave like they are in vertical orientation */
        case L_TABBED:
            return HORIZON;

        case L_DEFAULT:
            ERROR("Someone called con_orientation() on a con with L_DEFAULT, this is a bug in the code.");
            g_assert(false);

        case L_DOCK_AREA:
        case L_OUTPUT:
            ERROR("con_orientation() called on dockarea/output (%d) container %p", con->layout, con);
            g_assert(false);
    }
    /* should not be reached */
    g_assert(false);
}

GWMContainer *container_next_focused(GWMContainer *con)
{
    /* dock clients cannot be focused, so we focus the workspace instead */
    if (con->parent->type == CT_DOCK_AREA) {
        DEBUG("selecting workspace for dock client");
        return container_descend_focused(output_get_content(con->parent->parent));
    }

    if (container_is_floating(con)) {
        con = con->parent;
    }

    /* if 'con' is not the first entry in the focus stack, use the first one as
     * it’s currently focused already */
    GWMContainer* next = TAILQ_FIRST(&(con->parent->focusHead));
    if (next != con) {
        DEBUG("Using first entry %p", next);
    }
    else {
        /* try to focus the next container on the same level as this one or fall
         * back to its parent */
        if (!(next = TAILQ_NEXT(con, focused))) {
            next = con->parent;
        }
    }

    /* now go down the focus stack as far as
     * possible, excluding the current container */
    while (!TAILQ_EMPTY(&(next->focusHead)) && TAILQ_FIRST(&(next->focusHead)) != con) {
        next = TAILQ_FIRST(&(next->focusHead));
    }

    if (con->type == CT_FLOATING_CON && next != con->parent) {
        next = container_descend_focused(next);
    }

    return next;
}

GWMContainer *container_get_workspace(GWMContainer *con)
{
    GWMContainer* result = con;
    while (result != NULL && result->type != CT_WORKSPACE) {
        result = result->parent;
    }

    return result;
}

void container_update_parents_urgency(GWMContainer *con)
{
    GWMContainer* parent = con->parent;

    /* Urgency hints should not be set on any container higher up in the
     * hierarchy than the workspace level. Unfortunately, since the content
     * container has type == CT_CON, that’s not easy to verify in the loop
     * below, so we need another condition to catch that case: */
    if (con->type == CT_WORKSPACE) {
        return;
    }

    bool new_urgency_value = con->urgent;
    while (parent && parent->type != CT_WORKSPACE && parent->type != CT_DOCK_AREA) {
        if (new_urgency_value) {
            parent->urgent = true;
        }
        else {
            if (!container_has_urgent_child(parent)) {
                parent->urgent = false;
            }
        }
        parent = parent->parent;
    }
}

GWMContainer *container_by_window_id(xcb_window_t window)
{
    GWMContainer* con;
    TAILQ_FOREACH (con, &gAllContainer, allContainers) {
        if (con->window != NULL && con->window->id == window) {
            return con;
        }
    }

    return NULL;
}

GWMAdjacent container_adjacent_borders(GWMContainer *con)
{
    GWMAdjacent result = ADJ_NONE;

    if (container_is_floating(con)) {
        return result;
    }

    GWMContainer* workspace = container_get_workspace(con);
    if (con->rect.x == workspace->rect.x)
        result |= ADJ_LEFT_SCREEN_EDGE;
    if (con->rect.x + con->rect.width == workspace->rect.x + workspace->rect.width)
        result |= ADJ_RIGHT_SCREEN_EDGE;
    if (con->rect.y == workspace->rect.y)
        result |= ADJ_UPPER_SCREEN_EDGE;
    if (con->rect.y + con->rect.height == workspace->rect.y + workspace->rect.height)
        result |= ADJ_LOWER_SCREEN_EDGE;

    return result;
}

void container_set_urgency(GWMContainer *con, bool urgent)
{
    if (urgent && gFocused == con) {
        DEBUG("Ignoring urgency flag for current client");
        return;
    }

    const bool old_urgent = con->urgent;

    if (con->urgencyTimer == NULL) {
        con->urgent = urgent;
    }
    else {
        DEBUG("Discarding urgency WM_HINT because timer is running");
    }

    if (con->window) {
        if (con->urgent) {
            gettimeofday(&con->window->urgent, NULL);
        }
        else {
            con->window->urgent.tv_sec = 0;
            con->window->urgent.tv_usec = 0;
        }
    }

    container_update_parents_urgency(con);

    GWMContainer* ws;
    if ((ws = container_get_workspace(con)) != NULL) {
        workspace_update_urgent_flag(ws);
    }

    if (con->urgent != old_urgent) {
        INFO("Urgency flag changed to %d", con->urgent);
//        ipc_send_window_event("urgent", con);
    }
}

char *container_get_tree_representation(GWMContainer *con)
{
    if (container_is_leaf(con)) {
        if (!con->window) {
            return g_strdup("nowin");
        }

        if (!con->window->classInstance) {
            return g_strdup("noinstance");
        }
        return g_strdup(con->window->classInstance);
    }

    g_autofree char* buf = NULL;
    /* 1) add the Layout type to buf */
    if (con->layout == L_DEFAULT) {
        buf = g_strdup("D[");
    }
    else if (con->layout == L_SPLIT_V) {
        buf = g_strdup("V[");
    }
    else if (con->layout == L_SPLIT_H) {
        buf = g_strdup("H[");
    }
    else if (con->layout == L_TABBED) {
        buf = g_strdup("T[");
    }
    else if (con->layout == L_STACKED) {
        buf = g_strdup("S[");
    }
    else {
        ERROR("BUG: Code not updated to account for new layout type\n");
        g_assert(false);
    }

    /* 2) append representation of children */
    GWMContainer* child;
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        char *child_txt = container_get_tree_representation(child);

        char *tmp_buf = g_strdup_printf("%s%s%s", buf, (TAILQ_FIRST(&(con->nodesHead)) == child ? "" : " "), child_txt);
        free(buf);
        buf = tmp_buf;
        free(child_txt);
    }

    /* 3) close the brackets */
    char *complete_buf = g_strdup_printf ("%s]", buf);

    return complete_buf;
}

bool container_inside_stacked_or_tabbed(GWMContainer *con)
{
    if (con->parent == NULL) {
        return false;
    }

    if (con->parent->layout == L_STACKED ||
        con->parent->layout == L_TABBED) {
        return true;
    }

    return container_inside_stacked_or_tabbed(con->parent);
}

void container_unmark(GWMContainer *con, const char *name)
{
    GWMContainer* current;
    if (name == NULL) {
        DEBUG(_("Unmarking all containers."));
        TAILQ_FOREACH (current, &gAllContainer, allContainers) {
            if (con != NULL && current != con) {
                continue;
            }

            if (TAILQ_EMPTY(&(current->marksHead))) {
                continue;
            }

            GWMMark* mark;
            while (!TAILQ_EMPTY(&(current->marksHead))) {
                mark = TAILQ_FIRST(&(current->marksHead));
                FREE(mark->name);
                TAILQ_REMOVE(&(current->marksHead), mark, marks);
                FREE(mark);
//                ipc_send_window_event("mark", current);
            }
            current->markChanged = true;
        }
    }
    else {
        DEBUG(_("Removing mark \"%s\"."), name);
        current = (con == NULL) ? container_by_mark(name) : con;
        if (current == NULL) {
            DEBUG(_("No container found with this mark, so there is nothing to do."));
            return;
        }

        DEBUG(_("Found mark on con = %p. Removing it now."), current);
        current->markChanged = true;

        GWMMark* mark;
        TAILQ_FOREACH (mark, &(current->marksHead), marks) {
            if (strcmp(mark->name, name) != 0) {
                continue;
            }

            FREE(mark->name);
            TAILQ_REMOVE(&(current->marksHead), mark, marks);
            FREE(mark);
//            ipc_send_window_event("mark", current);
            break;
        }
    }
}

GWMContainer *container_inside_floating(GWMContainer *con)
{
    if (con == NULL) {
        return NULL;
    }

    if (con->type == CT_FLOATING_CON) {
        return con;
    }

    if (con->floating >= FLOATING_AUTO_ON) {
        return con->parent;
    }

    if (con->type == CT_WORKSPACE || con->type == CT_OUTPUT) {
        return NULL;
    }

    return container_inside_floating(con->parent);
}

GWMContainer **container_get_focus_order(GWMContainer *con)
{
    const int focusHeads = num_focus_heads(con);
    GWMContainer** focusOrder = g_malloc0(focusHeads * sizeof(GWMContainer*));
    GWMContainer* current;
    int idx = 0;
    TAILQ_FOREACH (current, &(con->focusHead), focused) {
        g_assert(idx < focusHeads);
        focusOrder[idx++] = current;
    }

    return focusOrder;
}

void container_force_split_parents_redraw(GWMContainer *con)
{
    GWMContainer* parent = con;

    while (parent != NULL && parent->type != CT_WORKSPACE && parent->type != CT_DOCK_AREA) {
        if (!container_is_leaf(parent)) {
            FREE(parent->decorationRenderParams);
        }

        parent = parent->parent;
    }
}

bool container_draw_decoration_into_frame(GWMContainer *con)
{
    return container_is_leaf(con) && container_border_style(con) == BS_NORMAL && (con->parent == NULL || (con->parent->layout != L_TABBED && con->parent->layout != L_STACKED));
}

bool container_has_mark(GWMContainer *con, const char *mark)
{
    GWMMark* current;
    TAILQ_FOREACH (current, &(con->marksHead), marks) {
        if (strcmp(current->name, mark) == 0) {
            return true;
        }
    }

    return false;
}

void container_set_layout(GWMContainer *con, GWMLayout layout)
{
    DEBUG("con_set_layout(%p, %d), con->type = %d", con, layout, con->type);

    if (con->type != CT_WORKSPACE) {
        con = con->parent;
    }

    if (con->layout == L_SPLIT_H || con->layout == L_SPLIT_V) {
        con->lastSplitLayout = con->layout;
    }

    if (con->type == CT_WORKSPACE) {
        if (container_num_children(con) == 0) {
            GWMLayout ws_layout = (layout == L_STACKED || layout == L_TABBED) ? layout : L_DEFAULT;
            DEBUG("Setting workspace_layout to %d\n", ws_layout);
            con->workspaceLayout = ws_layout;
            DEBUG("Setting layout to %d", layout);
            con->layout = layout;
        }
        else if (layout == L_STACKED || layout == L_TABBED || layout == L_SPLIT_V || layout == L_SPLIT_H) {
            DEBUG("Creating new split container\n");
            /* 1: create a new split container */
            GWMContainer* new = container_new(NULL, NULL);
            new->parent = con;

            /* 2: Set the requested layout on the split container and mark it as
             * split. */
            new->layout = layout;
            new->lastSplitLayout = con->lastSplitLayout;

            /* 3: move the existing cons of this workspace below the new con */
            GWMContainer** focus_order = get_focus_order(con);

            DEBUG("Moving cons");
            GWMContainer* child;
            while (!TAILQ_EMPTY(&(con->nodesHead))) {
                child = TAILQ_FIRST(&(con->nodesHead));
                container_detach(child);
                container_attach(child, new, true);
            }

            container_set_focus_order(new, focus_order);
            free(focus_order);

            /* 4: attach the new split container to the workspace */
            DEBUG(_("Attaching new split to ws"));
            container_attach(new, con, false);

            tree_flatten(gContainerRoot);
            container_force_split_parents_redraw(con);
            return;
        }
    }

    if (layout == L_DEFAULT) {
        con->layout = con->lastSplitLayout;
        if (con->layout == L_DEFAULT) {
            con->layout = L_SPLIT_H;
        }
    }
    else {
        con->layout = layout;
    }

    container_force_split_parents_redraw(con);
}

bool container_swap(GWMContainer *first, GWMContainer *second)
{
    g_assert(first != NULL);
    g_assert(second != NULL);
    DEBUG ("Swapping containers %p / %p", first, second);

    if (first->type != CT_CON) {
        ERROR("Only regular containers can be swapped, but found con = %p with type = %d.\n", first, first->type);
        return false;
    }

    if (second->type != CT_CON) {
        ERROR("Only regular containers can be swapped, but found con = %p with type = %d.\n", second, second->type);
        return false;
    }

    if (first == second) {
        DEBUG("Swapping container %p with itself, nothing to do.\n", first);
        return false;
    }

    if (container_has_parent(first, second) || container_has_parent(second, first)) {
        ERROR("Cannot swap containers %p and %p because they are in a parent-child relationship.\n", first, second);
        return false;
    }

    GWMContainer* ws1 = container_get_workspace(first);
    GWMContainer* ws2 = container_get_workspace(second);
    GWMContainer* restore_focus = NULL;
    if (ws1 == ws2 && ws1 == container_get_workspace(gFocused)) {
        /* Preserve focus in the current workspace. */
        restore_focus = gFocused;
    }
    else if (first == gFocused || container_has_parent(gFocused, first)) {
        restore_focus = second;
    }
    else if (second == gFocused || container_has_parent(gFocused, second)) {
        restore_focus = first;
    }

#define SWAP_CONS_IN_TREE(headname, field)                                  \
    do {                                                                    \
        struct headname *head1 = &(first->parent->headname);                \
        struct headname *head2 = &(second->parent->headname);               \
        GWMContainer* first_prev = TAILQ_PREV(first, headname, field);      \
        GWMContainer* second_prev = TAILQ_PREV(second, headname, field);    \
        if (second_prev == first) {                                         \
            TAILQ_SWAP(first, second, head1, field);                        \
        } else if (first_prev == second) {                                  \
            TAILQ_SWAP(second, first, head1, field);                        \
        } else {                                                            \
            TAILQ_REMOVE(head1, first, field);                              \
            TAILQ_REMOVE(head2, second, field);                             \
            if (second_prev == NULL) {                                      \
                TAILQ_INSERT_HEAD(head2, first, field);                     \
            } else {                                                        \
                TAILQ_INSERT_AFTER(head2, second_prev, first, field);       \
            }                                                               \
            if (first_prev == NULL) {                                       \
                TAILQ_INSERT_HEAD(head1, second, field);                    \
            } else {                                                        \
                TAILQ_INSERT_AFTER(head1, first_prev, second, field);       \
            }                                                               \
        }                                                                   \
    } while (0)

    SWAP_CONS_IN_TREE(nodesHead, nodes);
    SWAP_CONS_IN_TREE(focusHead, focused);
    SWAP(first->parent, second->parent, GWMContainer*);

    /* Floating nodes are children of CT_FLOATING_CONs, they are listed in
     * nodes_head and focus_head like all other containers. Thus, we don't need
     * to do anything special other than swapping the floating status and the
     * relevant rects. */
    SWAP(first->floating, second->floating, int);
    SWAP(first->rect, second->rect, GWMRect);
    SWAP(first->windowRect, second->windowRect, GWMRect);

    /* We need to copy each other's percentages to ensure that the geometry
     * doesn't change during the swap. */
    SWAP(first->percent, second->percent, double);

    if (restore_focus) {
        container_focus(restore_focus);
    }

    /* Update new parents' & workspaces' urgency. */
    container_set_urgency(first, first->urgent);
    container_set_urgency(second, second->urgent);

    /* Exchange fullscreen modes, can't use SWAP because we need to call the
     * correct functions. */
    GWMFullScreenMode second_fullscreen_mode = second->fullScreenMode;
    if (first->fullScreenMode == CF_NONE) {
        container_disable_full_screen(second);
    }
    else {
        container_enable_full_screen(second, first->fullScreenMode);
    }
    if (second_fullscreen_mode == CF_NONE) {
        container_disable_full_screen(first);
    }
    else {
        container_enable_full_screen(first, second_fullscreen_mode);
    }

    /* We don't actually need this since percentages-wise we haven't changed
     * anything, but we'll better be safe than sorry and just make sure as we'd
     * otherwise crash i3. */
    container_fix_percent(first->parent);
    container_fix_percent(second->parent);

    FREE(first->decorationRenderParams);
    FREE(second->decorationRenderParams);
    container_force_split_parents_redraw(first);
    container_force_split_parents_redraw(second);

    return true;
}

uint32_t container_rect_size_in_orientation(GWMContainer *con)
{
    return (container_orientation(con) == HORIZON ? con->rect.width : con->rect.height);
}

void container_merge_into(GWMContainer *old, GWMContainer *new)
{
    new->window = old->window;
    old->window = NULL;

    if (old->titleFormat) {
        FREE(new->titleFormat);
        new->titleFormat = old->titleFormat;
        old->titleFormat = NULL;
    }

    if (old->stickyGroup) {
        FREE(new->stickyGroup);
        new->stickyGroup = old->stickyGroup;
        old->stickyGroup = NULL;
    }

    new->sticky = old->sticky;

    container_set_urgency(new, old->urgent);

    GWMMark* mark;
    TAILQ_FOREACH (mark, &(old->marksHead), marks) {
        TAILQ_INSERT_TAIL(&(new->marksHead), mark, marks);
//        ipc_send_window_event("mark", new);
    }
    new->markChanged = (TAILQ_FIRST(&(old->marksHead)) != NULL);
    TAILQ_INIT(&(old->marksHead));

    tree_close_internal(old, KILL_WINDOW_DO_NOT, false);
}

bool container_full_screen_permits_focusing(GWMContainer *con)
{
    g_return_val_if_fail(gFocused, true);

    /* Find the first fullscreen ascendent. */
    GWMContainer* fs = gFocused;
    while (fs && fs->fullScreenMode == CF_NONE) {
        fs = fs->parent;
    }

    g_assert(fs != NULL);
    g_assert(fs->fullScreenMode != CF_NONE);

    if (fs->type == CT_WORKSPACE) {
        return true;
    }

    /* Allow it if the container itself is the fullscreen container. */
    if (con == fs) {
        return true;
    }

    /* If fullscreen is per-output, the focus being in a different workspace is
     * sufficient to guarantee that change won't leave fullscreen in bad shape. */
    if (fs->fullScreenMode == CF_OUTPUT && container_get_workspace(con) != container_get_workspace(fs)) {
        return true;
    }

    /* Allow it only if the container to be focused is contained within the
     * current fullscreen container. */
    return container_has_parent(con, fs);
}

bool container_move_to_mark(GWMContainer *con, const char *mark)
{
    GWMContainer* target = container_by_mark(mark);
    if (target == NULL) {
        DEBUG("found no container with mark \"%s\"", mark);
        return false;
    }

    return container_move_to_target(con, target);
}

void container_close(GWMContainer *con, GWMKillWindow killWindow)
{
    g_assert(con != NULL);
    DEBUG(_("Closing con = %p."), con);

    /* We never close output or root containers. */
    if (con->type == CT_OUTPUT || con->type == CT_ROOT) {
        DEBUG(_("con = %p is of type %d, not closing anything."), con, con->type);
        return;
    }

    if (con->type == CT_WORKSPACE) {
        DEBUG(_("con = %p is a workspace, closing all children instead."), con);
        GWMContainer* child, *nextChild;
        for (child = TAILQ_FIRST(&(con->focusHead)); child;) {
            nextChild = TAILQ_NEXT(child, focused);
            DEBUG(_("killing child = %p."), child);
            tree_close_internal(child, killWindow, false);
            child = nextChild;
        }

        return;
    }

    tree_close_internal(con, killWindow, false);
}

GWMContainer *container_descend_tiling_focused(GWMContainer *con)
{
    GWMContainer* next = con;
    GWMContainer* before;
    GWMContainer* child;

    g_return_val_if_fail(next, next);

    do {
        before = next;
        TAILQ_FOREACH (child, &(next->focusHead), focused) {
            if (child->type == CT_FLOATING_CON) {
                continue;
            }

            next = child;
            break;
        }
    } while (before != next && next != gFocused);

    return next;
}

bool container_has_parent(GWMContainer *con, GWMContainer *parent)
{
    GWMContainer* current = con->parent;
    if (current == NULL) {
        return false;
    }

    if (current == parent) {
        return true;
    }

    return container_has_parent(current, parent);
}

GWMContainer *container_new(GWMContainer *parent, GWMWindow *window)
{
    GWMContainer* newContainer = container_new_skeleton (parent, window);

    x_container_init(newContainer);

    return newContainer;
}

GWMContainer *container_get_full_screen_covering_ws(GWMContainer *ws)
{
    if (!ws) {
        return NULL;
    }

    GWMContainer* fs = container_get_full_screen_con(gContainerRoot, CF_GLOBAL);
    if (!fs) {
        return container_get_full_screen_con(ws, CF_OUTPUT);
    }

    return fs;
}

bool container_move_to_target(GWMContainer *con, GWMContainer *target)
{
    /* For target containers in the scratchpad, we just send the window to the scratchpad. */
    if (container_get_workspace(target) == workspace_get("__gwm_scratch")) {
        DEBUG(_("target container is in the scratchpad, moving container to scratchpad."));
        scratchpad_move(con);
        return true;
    }

    /* For floating target containers, we just send the window to the same workspace. */
    if (container_is_floating(target)) {
        DEBUG("target container is floating, moving container to target's workspace.");
        container_move_to_workspace(con, container_get_workspace(target), true, false, false);
        return true;
    }

    if (target->type == CT_WORKSPACE && container_is_leaf(target)) {
        DEBUG("target container is an empty workspace, simply moving the container there.");
        container_move_to_workspace(con, target, true, false, false);
        return true;
    }

    /* For split containers, we use the currently focused container within it.
     * This allows setting marks on, e.g., tabbed containers which will move
     * con to a new tab behind the focused tab. */
    if (container_is_split(target)) {
        DEBUG("target is a split container, descending to the currently focused child.");
        target = TAILQ_FIRST(&(target->focusHead));
    }

    if (con == target || container_has_parent(target, con)) {
        DEBUG("cannot move the container to or inside itself, aborting.");
        return false;
    }

    return _container_move_to_con(con, target, false, true, false, false, true);
}

void container_toggle_layout(GWMContainer *con, const char *toggleMode)
{
    GWMContainer* parent = con;
    /* Users can focus workspaces, but not any higher in the hierarchy.
     * Focus on the workspace is a special case, since in every other case, the
     * user means "change the layout of the parent split container". */
    if (con->type != CT_WORKSPACE) {
        parent = con->parent;
    }

    DEBUG("con_toggle_layout(%p, %s), parent = %p\n", con, toggleMode, parent);

    const char delim[] = " ";

    if (strcasecmp(toggleMode, "split") == 0 || strstr(toggleMode, delim)) {
        /* L_DEFAULT is used as a placeholder value to distinguish if
         * the first layout has already been saved. (it can never be L_DEFAULT) */
        GWMLayout new_layout = L_DEFAULT;
        bool current_layout_found = false;
        char *tm_dup = strdup(toggleMode);
        char *cur_tok = strtok(tm_dup, delim);

        for (GWMLayout layout; cur_tok != NULL; cur_tok = strtok(NULL, delim)) {
            if (strcasecmp(cur_tok, "split") == 0) {
                /* Toggle between splits. When the current layout is not a split
                 * layout, we just switch back to last_split_layout. Otherwise, we
                 * change to the opposite split layout. */
                if (parent->layout != L_SPLIT_H && parent->layout != L_SPLIT_V) {
                    layout = parent->lastSplitLayout;
                    /* In case last_split_layout was not initialized… */
                    if (layout == L_DEFAULT) {
                        layout = L_SPLIT_H;
                    }
                }
                else {
                    layout = (parent->layout == L_SPLIT_H) ? L_SPLIT_V : L_SPLIT_H;
                }
            }
            else {
                bool success = layout_from_name(cur_tok, &layout);
                if (!success || layout == L_DEFAULT) {
                    ERROR("The token '%s' was not recognized and has been skipped.", cur_tok);
                    continue;
                }
            }

            /* If none of the specified layouts match the current,
             * fall back to the first layout in the list */
            if (new_layout == L_DEFAULT) {
                new_layout = layout;
            }

            /* We found the active layout in the last iteration, so
             * now let's activate the current layout (next in list) */
            if (current_layout_found) {
                new_layout = layout;
                break;
            }

            if (parent->layout == layout) {
                current_layout_found = true;
            }
        }
        free(tm_dup);

        if (new_layout != L_DEFAULT) {
            container_set_layout(con, new_layout);
        }
    } else if (strcasecmp(toggleMode, "all") == 0 || strcasecmp(toggleMode, "default") == 0) {
        if (parent->layout == L_STACKED) {
            container_set_layout(con, L_TABBED);
        }
        else if (parent->layout == L_TABBED) {
            if (strcasecmp(toggleMode, "all") == 0) {
                container_set_layout(con, L_SPLIT_H);
            }
            else {
                container_set_layout(con, parent->lastSplitLayout);
            }
        }
        else if (parent->layout == L_SPLIT_H || parent->layout == L_SPLIT_V) {
            if (strcasecmp(toggleMode, "all") == 0) {
                if (parent->layout == L_SPLIT_H) {
                    container_set_layout(con, L_SPLIT_V);
                }
                else {
                    container_set_layout(con, L_STACKED);
                }
            }
            else {
                container_set_layout(con, L_STACKED);
            }
        }
    }
}

void container_toggle_full_screen(GWMContainer *con, int fullScreenMode)
{
    if (con->type == CT_WORKSPACE) {
        DEBUG(_("You cannot make a workspace fullscreen."));
        return;
    }

    DEBUG(_("toggling fullscreen for %p / %s"), con, con->name);

    if (con->fullScreenMode == CF_NONE) {
        container_enable_full_screen(con, fullScreenMode);
    }
    else {
        container_disable_full_screen(con);
    }
}

void container_mark(GWMContainer *con, const char *mark, GWMMarkMode mode)
{
    g_assert(con != NULL);
    DEBUG(_("Setting mark \"%s\" on con = %p."), mark, con);

    container_unmark(NULL, mark);
    if (mode == MM_REPLACE) {
        DEBUG(_("Removing all existing marks on con = %p."), con);

        GWMMark* current;
        while (!TAILQ_EMPTY(&(con->marksHead))) {
            current = TAILQ_FIRST(&(con->marksHead));
            container_unmark(con, current->name);
        }
    }

    GWMMark* new = g_malloc0(sizeof(GWMMark));
    new->name = g_strdup(mark);
    TAILQ_INSERT_TAIL(&(con->marksHead), new, marks);
//    ipc_send_window_event("mark", con);

    con->markChanged = true;
}

void container_set_focus_order(GWMContainer *con, GWMContainer **focusOrder)
{
    int focus_heads = 0;
    while (!TAILQ_EMPTY(&(con->focusHead))) {
        GWMContainer* current = TAILQ_FIRST(&(con->focusHead));
        TAILQ_REMOVE(&(con->focusHead), current, focused);
        focus_heads++;
    }

    for (int idx = 0; idx < focus_heads; idx++) {
        if (con->type != CT_WORKSPACE && container_inside_floating(focusOrder[idx])) {
            focus_heads++;
            continue;
        }
        TAILQ_INSERT_TAIL(&(con->focusHead), focusOrder[idx], focused);
    }
}

GWMContainer *container_new_skeleton(GWMContainer *parent, GWMWindow *window)
{
    GWMContainer* newC = g_malloc0(sizeof(GWMContainer));

    newC->onRemoveChild = container_on_remove_child;

    g_queue_push_tail (&gAllContainer, newC);

    newC->type = CT_CON;
    newC->window = window;
    newC->borderStyle = newC->maxUserBorderStyle = BS_NORMAL;
    newC->currentBorderWidth = -1;
    newC->windowIconPadding = -1;
    if (window) {
        newC->depth = window->depth;
    }
    else {
        newC->depth = gRootDepth;
    }
    DEBUG("opening window\n");

    TAILQ_INIT(&(newC->nodesHead));
    TAILQ_INIT(&(newC->focusHead));
    TAILQ_INIT(&(newC->marksHead));
    TAILQ_INIT(&(newC->swallowHead));
    TAILQ_INIT(&(newC->floatingHead));

    if (parent != NULL) {
        container_attach(newC, parent, false);
    }

    return newC;
}

void container_attach(GWMContainer *con, GWMContainer *parent, bool ignoreFocus)
{
    _container_attach (con, parent, NULL, ignoreFocus);
}

void container_mark_toggle(GWMContainer *con, const char *mark, GWMMarkMode mode)
{
    g_assert(con != NULL);

    DEBUG(_("Toggling mark \"%s\" on con = %p."), mark, con);

    if (container_has_mark(con, mark)) {
        container_unmark(con, mark);
    }
    else {
        container_mark(con, mark, mode);
    }
}

bool container_find_transient_for_window(GWMContainer *start, xcb_window_t target)
{
    GWMContainer* transient_con = start;
    int count = container_num_windows(gContainerRoot);
    while (transient_con != NULL && transient_con->window != NULL && transient_con->window->transientFor != XCB_NONE) {
        DEBUG(_("transient_con = 0x%08x, transient_con->window->transient_for = 0x%08x, target = 0x%08x"),
                transient_con->window->id, transient_con->window->transientFor, target);
        if (transient_con->window->transientFor == target) {
            return true;
        }

        GWMContainer* next_transient = container_by_window_id(transient_con->window->transientFor);
        if (next_transient == NULL) {
            break;
        }

        if (transient_con == next_transient) {
            break;
        }
        transient_con = next_transient;

        if (count-- <= 0) {
            break;
        }
    }

    return false;
}

GWMContainer *container_descend_direction(GWMContainer *con, GWMDirection direction)
{
    GWMContainer* most = NULL;
    GWMContainer* current;
    int orientation = container_orientation(con);

    DEBUG("con_descend_direction(%p, orientation %d, direction %d)", con, orientation, direction);

    if (direction == D_LEFT || direction == D_RIGHT) {
        if (orientation == HORIZON) {
            /* If the direction is horizontal, we can use either the first
             * (D_RIGHT) or the last con (D_LEFT) */
            if (direction == D_RIGHT) {
                most = TAILQ_FIRST(&(con->nodesHead));
            }
            else {
                most = TAILQ_LAST(&(con->nodesHead), nodesHead);
            }
        }
        else if (orientation == VERT) {
            /* Wrong orientation. We use the last focused con. Within that con,
             * we recurse to chose the left/right con or at least the last
             * focused one. */
            TAILQ_FOREACH (current, &(con->focusHead), focused) {
                if (current->type != CT_FLOATING_CON) {
                    most = current;
                    break;
                }
            }
        }
        else {
            /* If the con has no orientation set, it’s not a split container
             * but a container with a client window, so stop recursing */
            return con;
        }
    }

    if (direction == D_UP || direction == D_DOWN) {
        if (orientation == VERT) {
            /* If the direction is vertical, we can use either the first
             * (D_DOWN) or the last con (D_UP) */
            if (direction == D_UP) {
                most = TAILQ_LAST(&(con->nodesHead), nodesHead);
            }
            else {
                most = TAILQ_FIRST(&(con->nodesHead));
            }
        }
        else if (orientation == HORIZON) {
            /* Wrong orientation. We use the last focused con. Within that con,
             * we recurse to chose the top/bottom con or at least the last
             * focused one. */
            TAILQ_FOREACH (current, &(con->focusHead), focused) {
                if (current->type != CT_FLOATING_CON) {
                    most = current;
                    break;
                }
            }
        }
        else {
            /* If the con has no orientation set, it’s not a split container
             * but a container with a client window, so stop recursing */
            return con;
        }
    }

    g_return_val_if_fail(most, con);

    return container_descend_direction(most, direction);
}

void container_enable_full_screen(GWMContainer *con, GWMFullScreenMode fullScreenMode)
{
    if (con->type == CT_WORKSPACE) {
        DEBUG(_("You cannot make a workspace fullscreen."));
        return;
    }

    g_assert(fullScreenMode == CF_GLOBAL || fullScreenMode == CF_OUTPUT);

    if (fullScreenMode == CF_GLOBAL) {
        DEBUG(_("enabling global fullscreen for %p / %s"), con, con->name);
    }
    else {
        DEBUG(_("enabling fullscreen for %p / %s"), con, con->name);
    }

    if (con->fullScreenMode == fullScreenMode) {
        DEBUG(_("fullscreen already enabled for %p / %s"), con, con->name);
        return;
    }

    GWMContainer* container_ws = container_get_workspace(con);

    /* Disable any fullscreen container that would conflict the new one. */
    GWMContainer* fullscreen = container_get_full_screen_con(gContainerRoot, CF_GLOBAL);
    if (fullscreen == NULL) {
        fullscreen = container_get_full_screen_con(container_ws, CF_OUTPUT);
    }

    if (fullscreen != NULL) {
        container_disable_full_screen(fullscreen);
    }

    GWMContainer* cur_ws = container_get_workspace(gFocused);
    GWMContainer* old_focused = gFocused;
    if (fullScreenMode == CF_GLOBAL && cur_ws != container_ws) {
        workspace_show(container_ws);
    }

    container_activate(con);
    if (fullScreenMode != CF_GLOBAL && cur_ws != container_ws) {
        container_activate(old_focused);
    }

    container_set_fullscreen_mode(con, fullScreenMode);
}

void container_move_to_output(GWMContainer *con, GWMOutput *output, bool fixCoordinates)
{
    GWMContainer* ws = NULL;
    GREP_FIRST(ws, output_get_content(output->container), workspace_is_visible(child));
    g_assert(ws != NULL);
    DEBUG("Moving con %p to output %s\n", con, output_primary_name(output));
    container_move_to_workspace(con, ws, fixCoordinates, false, false);
}

GWMContainer *container_for_window(GWMContainer *con, GWMWindow *window, GWMMatch **storeMatch)
{
    GWMContainer* child;
    GWMMatch *match;

    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        TAILQ_FOREACH (match, &(child->swallowHead), matches) {
            if (!match_matches_window(match, window)) {
                continue;
            }

            if (storeMatch != NULL) {
                *storeMatch = match;
            }
            return child;
        }

        GWMContainer* result = container_for_window(child, window, storeMatch);
        if (result != NULL) {
            return result;
        }
    }

    TAILQ_FOREACH (child, &(con->floatingHead), floatingWindows) {
        TAILQ_FOREACH (match, &(child->swallowHead), matches) {
            if (!match_matches_window(match, window)) {
                continue;
            }
            if (storeMatch != NULL) {
                *storeMatch = match;
            }
            return child;
        }

        GWMContainer* result = container_for_window(child, window, storeMatch);
        if (result != NULL) {
            return result;
        }
    }

    return NULL;
}

bool container_move_to_output_name(GWMContainer *con, const char *name, bool fixCoordinates)
{
    GWMOutput *current_output = output_get_output_for_con(con);
    GWMOutput *output = output_get_output_from_string(current_output, name);
    if (output == NULL) {
        ERROR("Could not find output \"%s\"", name);
        return false;
    }

    container_move_to_output(con, output, fixCoordinates);

    return true;
}

GWMContainer *container_parent_with_orientation(GWMContainer *con, GWMOrientation orientation)
{
    DEBUG(_("Searching for parent of Con %p with orientation %d"), con, orientation);
    GWMContainer* parent = con->parent;
    if (parent->type == CT_FLOATING_CON) {
        return NULL;
    }

    while (container_orientation(parent) != orientation) {
        DEBUG(_("Need to go one level further up"));
        parent = parent->parent;
        /* Abort when we reach a floating con, or an output con */
        if (parent && (parent->type == CT_FLOATING_CON || parent->type == CT_OUTPUT || (parent->parent && parent->parent->type == CT_OUTPUT))) {
            parent = NULL;
        }

        if (parent == NULL) {
            break;
        }
    }

    DEBUG(_("Result: %p"), parent);

    return parent;
}

void container_set_border_style(GWMContainer *con, GWMBorderStyle borderStyle, int borderWidth)
{
    if (borderStyle > con->maxUserBorderStyle) {
        borderStyle = con->maxUserBorderStyle;
    }

    /* Handle the simple case: non-floating containerns */
    if (!container_is_floating(con)) {
        con->borderStyle = borderStyle;
        con->currentBorderWidth = borderWidth;
        return;
    }

    /* For floating containers, we want to keep the position/size of the
     * *window* itself. We first add the border pixels to con->rect to make
     * con->rect represent the absolute position of the window (same for
     * parent). Then, we change the border style and subtract the new border
     * pixels. For the parent, we do the same also for the decoration. */
    GWMContainer* parent = con->parent;
    GWMRect bsr = container_border_style_rect(con);

    con->rect = util_rect_add(con->rect, bsr);
    parent->rect = util_rect_add(parent->rect, bsr);

    /* Change the border style, get new border/decoration values. */
    con->borderStyle = borderStyle;
    con->currentBorderWidth = borderWidth;
    bsr = container_border_style_rect(con);

    con->rect = util_rect_sub(con->rect, bsr);
    parent->rect = util_rect_sub(parent->rect, bsr);
}

GWMContainer* container_get_full_screen_con(GWMContainer *con, GWMFullScreenMode fullScreenMode)
{
    GWMContainer* current, *child;

    /* TODO: is breadth-first-search really appropriate? (check as soon as
     * fullscreen levels and fullscreen for containers is implemented) */
    TAILQ_HEAD(bfs_head, bfs_entry) bfs_head = TAILQ_HEAD_INITIALIZER(bfs_head);
    struct bfs_entry *entry = g_malloc0(sizeof(struct bfs_entry));
    entry->con = con;
    TAILQ_INSERT_TAIL(&bfs_head, entry, entries);

    while (!TAILQ_EMPTY(&bfs_head)) {
        entry = TAILQ_FIRST(&bfs_head);
        current = entry->con;
        if (current != con && current->fullScreenMode == fullScreenMode) {
            while (!TAILQ_EMPTY(&bfs_head)) {
                entry = TAILQ_FIRST(&bfs_head);
                TAILQ_REMOVE(&bfs_head, entry, entries);
                free(entry);
            }
            return current;
        }

        TAILQ_REMOVE(&bfs_head, entry, entries);
        free(entry);

        TAILQ_FOREACH (child, &(current->nodesHead), nodes) {
            entry = g_malloc0(sizeof(struct bfs_entry));
            entry->con = child;
            TAILQ_INSERT_TAIL(&bfs_head, entry, entries);
        }

        TAILQ_FOREACH (child, &(current->floatingHead), floatingWindows) {
            entry = g_malloc0(sizeof(struct bfs_entry));
            entry->con = child;
            TAILQ_INSERT_TAIL(&bfs_head, entry, entries);
        }
    }

    return NULL;
}

void container_move_to_workspace(GWMContainer *con, GWMContainer *workspace, bool fixCoordinates, bool doNotWarp, bool ignoreFocus)
{
    g_assert(workspace->type == CT_WORKSPACE);

    GWMContainer* source_ws = container_get_workspace(con);
    if (workspace == source_ws) {
        DEBUG("Not moving, already there");
        return;
    }

    GWMContainer* target = container_descend_focused(workspace);
    _container_move_to_con(con, target, true, fixCoordinates, doNotWarp, ignoreFocus, true);
}

bool container_is_hidden(GWMContainer *con)
{
    GWMContainer* current = con;

    while (current != NULL && current->type != CT_WORKSPACE) {
        GWMContainer* parent = current->parent;
        if (parent != NULL && (parent->layout == L_TABBED || parent->layout == L_STACKED)) {
            if (TAILQ_FIRST(&(parent->focusHead)) != current) {
                return true;
            }
        }
        current = parent;
    }

    return false;
}


static void container_on_remove_child(GWMContainer* con)
{
    DEBUG("on_remove_child");

    /* Every container 'above' (in the hierarchy) the workspace content should
     * not be closed when the last child was removed */
    if (con->type == CT_OUTPUT || con->type == CT_ROOT || con->type == CT_DOCK_AREA || (con->parent != NULL && con->parent->type == CT_OUTPUT)) {
        DEBUG("not handling, type = %d, name = %s", con->type, con->name);
        return;
    }

    /* For workspaces, close them only if they're not visible anymore */
    if (con->type == CT_WORKSPACE) {
        if (TAILQ_EMPTY(&(con->focusHead)) && !workspace_is_visible(con)) {
            INFO("Closing old workspace (%p / %s), it is empty", con, con->name);
//            yajl_gen gen = ipc_marshal_workspace_event("empty", con, NULL);
            tree_close_internal(con, KILL_WINDOW_DO_NOT, false);

            const unsigned char *payload;
//            ylength length;
//            y(get_buf, &payload, &length);
//            ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, (const char *)payload);

//            y(free);
        }
        return;
    }

    container_force_split_parents_redraw(con);
    con->urgent = container_has_urgent_child(con);
    container_update_parents_urgency(con);

    /* TODO: check if this container would swallow any other client and
     * don’t close it automatically. */
    int children = container_num_children(con);
    if (children == 0) {
        DEBUG("Container empty, closing\n");
        tree_close_internal(con, KILL_WINDOW_DO_NOT, false);
        return;
    }
}


static void _container_attach(GWMContainer* con, GWMContainer* parent, GWMContainer* previous, bool ignoreFocus)
{
    con->parent = parent;
    GWMContainer* loop;
    GWMContainer* current = previous;
    struct nodesHead *nodes_head = &(parent->nodesHead);
    struct focusHead *focus_head = &(parent->focusHead);

    /* Workspaces are handled differently: they need to be inserted at the
     * right position. */
    if (con->type == CT_WORKSPACE) {
        DEBUG(_("it's a workspace. num = %d"), con->workspaceNum);
        if (con->workspaceNum == -1 || TAILQ_EMPTY(nodes_head)) {
            TAILQ_INSERT_TAIL(nodes_head, con, nodes);
        }
        else {
            current = TAILQ_FIRST(nodes_head);
            if (con->workspaceNum < current->workspaceNum) {
                /* we need to insert the container at the beginning */
                TAILQ_INSERT_HEAD(nodes_head, con, nodes);
            }
            else {
                while (current->workspaceNum != -1 && con->workspaceNum > current->workspaceNum) {
                    current = TAILQ_NEXT(current, nodes);
                    if (current == TAILQ_END(nodes_head)) {
                        current = NULL;
                        break;
                    }
                }
                /* we need to insert con after current, if current is not NULL */
                if (current) {
                    TAILQ_INSERT_BEFORE(current, con, nodes);
                }
                else {
                    TAILQ_INSERT_TAIL(nodes_head, con, nodes);
                }
            }
        }
        goto add_to_focus_head;
    }

    if (parent->type == CT_DOCK_AREA) {
        /* Insert dock client, sorting alphanumerically by class and then
         * instance name. This makes dock client order deterministic. As a side
         * effect, bars without a custom bar id will be sorted according to
         * their declaration order in the config file. See #3491. */
        current = NULL;
        TAILQ_FOREACH (loop, nodes_head, nodes) {
            int result = g_ascii_strcasecmp (con->window->classClass, loop->window->classClass);
            if (result == 0) {
                result = g_ascii_strcasecmp (con->window->classInstance, loop->window->classInstance);
            }
            if (result < 0) {
                current = loop;
                break;
            }
        }
        if (current) {
            TAILQ_INSERT_BEFORE(loop, con, nodes);
        }
        else {
            TAILQ_INSERT_TAIL(nodes_head, con, nodes);
        }
        goto add_to_focus_head;
    }

    if (con->type == CT_FLOATING_CON) {
        DEBUG(_("Inserting into floating containers"));
        TAILQ_INSERT_TAIL(&(parent->floatingHead), con, floatingWindows);
    }
    else {
        if (!ignoreFocus) {
            /* Get the first tiling container in focus stack */
            TAILQ_FOREACH (loop, &(parent->focusHead), focused) {
                if (loop->type == CT_FLOATING_CON) {
                    continue;
                }
                current = loop;
                break;
            }
        }

        /* When the container is not a split container (but contains a window)
         * and is attached to a workspace, we check if the user configured a
         * workspace_layout. This is done in workspace_attach_to, which will
         * provide us with the container to which we should attach (either the
         * workspace or a new split container with the configured
         * workspace_layout).
         */
        if (con->window != NULL &&
            parent->type == CT_WORKSPACE &&
            parent->workspaceLayout != L_DEFAULT) {
            DEBUG(_("Parent is a workspace. Applying default layout..."));
            GWMContainer* target = workspace_attach_to(parent);

            /* Attach the original con to this new split con instead */
            nodes_head = &(target->nodesHead);
            focus_head = &(target->focusHead);
            con->parent = target;
            current = NULL;

            DEBUG(_("done"));
        }

        /* Insert the container after the tiling container, if found.
         * When adding to a CT_OUTPUT, just append one after another. */
        if (current != NULL && parent->type != CT_OUTPUT) {
            DEBUG(_("Inserting con = %p after con %p"), con, current);
            TAILQ_INSERT_AFTER(nodes_head, current, con, nodes);
        }
        else {
            TAILQ_INSERT_TAIL(nodes_head, con, nodes);
        }
    }

add_to_focus_head:
    /* We insert to the TAIL because container_focus() will correct this.
     * This way, we have the option to insert Cons without having
     * to focus them. */
    TAILQ_INSERT_TAIL(focus_head, con, focused);
    container_force_split_parents_redraw(con);
}

static void container_raise(GWMContainer* con)
{
    GWMContainer* floating = container_inside_floating(con);
    if (floating) {
        floating_raise_con(floating);
    }
}

static int num_focus_heads(GWMContainer* con)
{
    int focus_heads = 0;

    GWMContainer* current;
    TAILQ_FOREACH (current, &(con->focusHead), focused) {
        focus_heads++;
    }

    return focus_heads;
}

static void container_set_fullscreen_mode(GWMContainer* con, GWMFullScreenMode fullscreenMode)
{
    con->fullScreenMode = fullscreenMode;

    DEBUG(_("mode now: %d"), con->fullScreenMode);

    /* Send an ipc window "fullscreen_mode" event */
//    ipc_send_window_event("fullscreen_mode", con);

    g_return_if_fail(con->window);

    if (con->fullScreenMode != CF_NONE) {
        DEBUG(_("Setting _NET_WM_STATE_FULLSCREEN for con = %p / window = %d."), con, con->window->id);
        xcb_gwm_add_property_atom(gConn, con->window->id, A__NET_WM_STATE, A__NET_WM_STATE_FULLSCREEN);
    }
    else {
        DEBUG(_("Removing _NET_WM_STATE_FULLSCREEN for con = %p / window = %d."), con, con->window->id);
        xcb_gwm_remove_property_atom(gConn, con->window->id, A__NET_WM_STATE, A__NET_WM_STATE_FULLSCREEN);
    }
}

static bool _container_move_to_con(GWMContainer* con, GWMContainer* target, bool behindFocused, bool fixCoordinates, bool dontWarp, bool ignoreFocus, bool fixPercentage)
{
    GWMContainer* orig_target = target;

    /* Prevent moving if this would violate the fullscreen focus restrictions. */
    GWMContainer* target_ws = container_get_workspace(target);
    if (!ignoreFocus && !container_full_screen_permits_focusing(target_ws)) {
        DEBUG(_("Cannot move out of a fullscreen container."));
        return false;
    }

    if (container_is_floating(con)) {
        DEBUG("Container is floating, using parent instead.\n");
        con = con->parent;
    }

    GWMContainer* source_ws = container_get_workspace(con);

    if (con->type == CT_WORKSPACE) {
        /* Re-parent all of the old workspace's floating windows. */
        GWMContainer* child;
        while (!TAILQ_EMPTY(&(source_ws->floatingHead))) {
            child = TAILQ_FIRST(&(source_ws->floatingHead));
            container_move_to_workspace(child, target_ws, true, true, false);
        }

        /* If there are no non-floating children, ignore the workspace. */
        if (container_is_leaf(con))
            return false;

        con = workspace_encapsulate(con);
        if (con == NULL) {
            ERROR(_("Workspace failed to move its contents into a container!"));
            return false;
        }
    }

    /* Save the urgency state so that we can restore it. */
    bool urgent = con->urgent;

    /* Save the current workspace. So we can call workspace_show() by the end
     * of this function. */
    GWMContainer *current_ws = container_get_workspace(gFocused);

    GWMContainer* source_output = container_get_output(con),
        *dest_output = container_get_output(target_ws);

    /* 1: save the container which is going to be focused after the current
     * container is moved away */
    GWMContainer* focus_next = NULL;
    if (!ignoreFocus && source_ws == current_ws && target_ws != source_ws) {
        focus_next = container_descend_focused(source_ws);
        if (focus_next == con || container_has_parent(focus_next, con)) {
            focus_next = container_next_focused(con);
        }
    }

    /* 2: we go up one level, but only when target is a normal container */
    if (target->type != CT_WORKSPACE) {
        DEBUG("target originally = %p / %s / type %d\n", target, target->name, target->type);
        target = target->parent;
    }

    /* 3: if the original target is the direct child of a floating container, we
     * can't move con next to it - floating containers have only one child - so
     * we get the workspace instead. */
    if (target->type == CT_FLOATING_CON) {
        DEBUG("floatingcon, going up even further\n");
        orig_target = target;
        target = target->parent;
    }

    if (con->type == CT_FLOATING_CON) {
        GWMContainer* ws = container_get_workspace(target);
        DEBUG("This is a floating window, using workspace %p / %s\n", ws, ws->name);
        target = ws;
    }

    if (source_output != dest_output) {
        /* Take the relative coordinates of the current output, then add them
         * to the coordinate space of the correct output */
        if (fixCoordinates && con->type == CT_FLOATING_CON) {
            floating_fix_coordinates(con, &(source_output->rect), &(dest_output->rect));
        }
        else {
            DEBUG("Not fixing coordinates, fix_coordinates flag = %d\n", fixCoordinates);
        }
    }

    /* If moving a fullscreen container and the destination already has a
     * fullscreen window on it, un-fullscreen the target's fullscreen con.
     * con->fullscreen_mode is not enough in some edge cases:
     * 1. con is CT_FLOATING_CON, child is fullscreen.
     * 2. con is the parent of a fullscreen container, can be triggered by
     * moving the parent with command criteria.
     */
    GWMContainer* fullscreen = container_get_full_screen_con(target_ws, CF_OUTPUT);
    const bool container_has_fullscreen = con->fullScreenMode != CF_NONE || container_get_full_screen_con(con, CF_GLOBAL) || container_get_full_screen_con(con, CF_OUTPUT);
    if (container_has_fullscreen && fullscreen != NULL) {
        container_toggle_full_screen(fullscreen, CF_OUTPUT);
        fullscreen = NULL;
    }

    DEBUG("Re-attaching container to %p / %s\n", target, target->name);
    /* 4: re-attach the con to the parent of this focused container */
    GWMContainer* parent = con->parent;
    container_detach(con);
    _container_attach(con, target, behindFocused ? NULL : orig_target, !behindFocused);

    /* 5: fix the percentages */
    if (fixPercentage) {
        container_fix_percent(parent);
        con->percent = 0.0;
        container_fix_percent(target);
    }

    /* 6: focus the con on the target workspace, but only within that
     * workspace, that is, don’t move focus away if the target workspace is
     * invisible.
     * We don’t focus the con for i3 pseudo workspaces like __i3_scratch and
     * we don’t focus when there is a fullscreen con on that workspace. We
     * also don't do it if the caller requested to ignore focus. */
    if (!ignoreFocus && !container_is_internal(target_ws) && !fullscreen) {
        /* We need to save the focused workspace on the output in case the
         * new workspace is hidden and it's necessary to immediately switch
         * back to the originally-focused workspace. */
        GWMContainer* old_focus_ws = TAILQ_FIRST(&(output_get_content(dest_output)->focusHead));
        GWMContainer* old_focus = gFocused;
        container_activate(container_descend_focused(con));

        if (old_focus_ws == current_ws && old_focus->type != CT_WORKSPACE) {
            /* Restore focus to the currently focused container. */
            container_activate(old_focus);
        }
        else if (container_get_workspace(gFocused) != old_focus_ws) {
            /* Restore focus if the output's focused workspace has changed. */
            container_focus(container_descend_focused(old_focus_ws));
        }
    }

    /* 7: when moving to another workspace, we leave the focus on the current
     * workspace. (see also #809) */
    if (!ignoreFocus) {
        workspace_show(current_ws);
        if (dontWarp) {
            DEBUG("x_set_warp_to(NULL) because dont_warp is set\n");
            x_set_warp_to(NULL);
        }
    }

    /* Set focus only if con was on current workspace before moving.
     * Otherwise we would give focus to some window on different workspace. */
    if (focus_next) {
        container_activate(container_descend_focused(focus_next));
    }

    /* 8. If anything within the container is associated with a startup sequence,
     * delete it so child windows won't be created on the old workspace. */
    if (!container_is_leaf(con)) {
        GWMContainer* child;
        TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
            if (!child->window) {
                continue;
            }

            startup_sequence_delete_by_window(child->window);
        }
    }

    if (con->window) {
        startup_sequence_delete_by_window(con->window);
    }

    /* 9. If the container was marked urgent, move the urgency hint. */
    if (urgent) {
        workspace_update_urgent_flag(source_ws);
        container_set_urgency(con, true);
    }

    /* Ensure the container will be redrawn. */
    FREE(con->decorationRenderParams);

    CALL(parent, onRemoveChild);

//    ipc_send_window_event("move", con);
    extend_wm_hint_update_wm_desktop();

    return true;
}

static bool has_outer_gaps(GWMGaps gaps)
{
    return gaps.top > 0 || gaps.right > 0 || gaps.bottom > 0 || gaps.left > 0;
}

static GWMRect con_border_style_rect_without_title(GWMContainer* con)
{
    return (GWMRect){0, 0, 0, 0};
}
