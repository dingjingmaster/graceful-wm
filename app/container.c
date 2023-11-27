//
// Created by dingjing on 23-11-24.
//

#include "container.h"

void container_activate(GWMContainer *con)
{

}

GWMContainer *container_descend_focused(GWMContainer *con)
{
    return NULL;
}

void container_free(GWMContainer *con)
{

}

void container_focus(GWMContainer *con)
{

}

bool container_exists(GWMContainer *con)
{
    return 0;
}

void container_detach(GWMContainer *con)
{

}

bool container_is_leaf(GWMContainer *con)
{
    return 0;
}

bool container_is_split(GWMContainer *con)
{
    return 0;
}

bool container_is_sticky(GWMContainer *con)
{
    return 0;
}

bool container_is_docked(GWMContainer *con)
{
    return 0;
}

int container_num_windows(GWMContainer *con)
{
    return 0;
}

int container_border_style(GWMContainer *con)
{
    return 0;
}

void container_fix_percent(GWMContainer *con)
{

}

bool container_is_internal(GWMContainer *con)
{
    return 0;
}

bool container_is_floating(GWMContainer *con)
{
    return 0;
}

int container_num_children(GWMContainer *con)
{
    return 0;
}

bool container_has_children(GWMContainer *con)
{
    return 0;
}

bool container_inside_focused(GWMContainer *con)
{
    return 0;
}

bool container_accepts_window(GWMContainer *con)
{
    return 0;
}

GWMContainer *container_by_mark(const char *mark)
{
    return NULL;
}

bool container_has_urgent_child(GWMContainer *con)
{
    return 0;
}

void container_activate_unblock(GWMContainer *con)
{

}

GWMRect container_minimum_size(GWMContainer *con)
{
    GWMRect result;
    return result;
}

GWMContainer *container_by_container_id(long target)
{
    return NULL;
}

bool container_has_managed_window(GWMContainer *con)
{
    return 0;
}

void container_disable_full_screen(GWMContainer *con)
{

}

int container_num_visible_children(GWMContainer *con)
{
    return 0;
}

char *container_parse_title_format(GWMContainer *con)
{
    return NULL;
}

GWMContainer *container_get_output(GWMContainer *con)
{
    return NULL;
}

GWMRect container_border_style_rect(GWMContainer *con)
{
    GWMRect result;
    return result;
}

GWMContainer *container_by_frame_id(xcb_window_t frame)
{
    return NULL;
}

GWMOrientation container_orientation(GWMContainer *con)
{
    return NO_ORIENTATION;
}

GWMContainer *container_next_focused(GWMContainer *con)
{
    return NULL;
}

GWMContainer *container_get_workspace(GWMContainer *con)
{
    return NULL;
}

void container_update_parents_urgency(GWMContainer *con)
{

}

GWMContainer *container_by_window_id(xcb_window_t window)
{
    return NULL;
}

GWMAdjacent container_adjacent_borders(GWMContainer *con)
{
    return ADJ_RIGHT_SCREEN_EDGE;
}

void container_set_urgency(GWMContainer *con, bool urgent)
{

}

char *container_get_tree_representation(GWMContainer *con)
{
    return NULL;
}

bool container_inside_stacked_or_tabbed(GWMContainer *con)
{
    return 0;
}

void container_unmark(GWMContainer *con, const char *name)
{

}

GWMContainer *container_inside_floating(GWMContainer *con)
{
    return NULL;
}

GWMContainer **container_get_focus_order(GWMContainer *con)
{
    return NULL;
}

void container_force_split_parents_redraw(GWMContainer *con)
{

}

bool container_draw_decoration_into_frame(GWMContainer *con)
{
    return 0;
}

bool container_has_mark(GWMContainer *con, const char *mark)
{
    return 0;
}

void container_set_layout(GWMContainer *con, GWMLayout layout)
{

}

bool container_swap(GWMContainer *first, GWMContainer *second)
{
    return 0;
}

uint32_t container_rect_size_in_orientation(GWMContainer *con)
{
    return 0;
}

void container_merge_into(GWMContainer *old, GWMContainer *new)
{

}

bool container_full_screen_permits_focusing(GWMContainer *con)
{
    return 0;
}

bool container_move_to_mark(GWMContainer *con, const char *mark)
{
    return 0;
}

void container_close(GWMContainer *con, GWMKillWindow killWindow)
{

}

GWMContainer *container_descend_tiling_focused(GWMContainer *con)
{
    return NULL;
}

bool container_has_parent(GWMContainer *con, GWMContainer *parent)
{
    return 0;
}

GWMContainer *container_new(GWMContainer *parent, GWMWindow *window)
{
    return NULL;
}

GWMContainer *container_get_full_screen_covering_ws(GWMContainer *ws)
{
    return NULL;
}

bool container_move_to_target(GWMContainer *con, GWMContainer *target)
{
    return 0;
}

void container_toggle_layout(GWMContainer *con, const char *toggleMode)
{

}

void container_toggle_full_screen(GWMContainer *con, int fullScreenMode)
{

}

void container_mark(GWMContainer *con, const char *mark, GWMMarkMode mode)
{

}

void container_set_focus_order(GWMContainer *con, GWMContainer **focusOrder)
{

}

GWMContainer *container_new_skeleton(GWMContainer *parent, GWMWindow *window)
{
    return NULL;
}

void container_attach(GWMContainer *con, GWMContainer *parent, bool ignoreFocus)
{

}

void container_mark_toggle(GWMContainer *con, const char *mark, GWMMarkMode mode)
{

}

bool container_find_transient_for_window(GWMContainer *start, xcb_window_t target)
{
    return 0;
}

GWMContainer *container_descend_direction(GWMContainer *con, GWMDirection direction)
{
    return NULL;
}

void container_enable_full_screen(GWMContainer *con, GWMFullScreenMode fullScreenMode)
{

}

void container_move_to_output(GWMContainer *con, GWMOutput *output, bool fixCoordinates)
{

}

GWMContainer *container_for_window(GWMContainer *con, GWMWindow *window, Match **storeMatch)
{
    return NULL;
}

bool container_move_to_output_name(GWMContainer *con, const char *name, bool fixCoordinates)
{
    return 0;
}

GWMContainer *container_parent_with_orientation(GWMContainer *con, GWMOrientation orientation)
{
    return NULL;
}

void container_set_border_style(GWMContainer *con, GWMBorderStyle borderStyle, int borderWidth)
{

}

GWMContainer *container_get_full_screen_con(GWMContainer *con, GWMFullScreenMode fullScreenMode)
{
    return NULL;
}

void container_move_to_workspace(GWMContainer *con, GWMContainer *workspace, bool fixCoordinates, bool doNotWarp, bool ignoreFocus)
{

}
