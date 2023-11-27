//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_CONTAINER_H
#define GRACEFUL_WM_CONTAINER_H
#include <ev.h>
#include <glib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cairo/cairo.h>

#include "types.h"


void container_free(GWMContainer* con);
void container_focus(GWMContainer* con);
bool container_exists(GWMContainer* con);
void container_detach(GWMContainer* con);
bool container_is_leaf(GWMContainer* con);
bool container_is_split(GWMContainer* con);
void container_activate(GWMContainer* con);
bool container_is_sticky(GWMContainer* con);
bool container_is_docked(GWMContainer* con);
int container_num_windows(GWMContainer* con);
int container_border_style(GWMContainer*con);
void container_fix_percent(GWMContainer* con);
bool container_is_internal(GWMContainer* con);
bool container_is_floating(GWMContainer* con);
int container_num_children(GWMContainer* con);
bool container_has_children(GWMContainer* con);
bool container_inside_focused(GWMContainer* con);
bool container_accepts_window(GWMContainer* con);
GWMContainer* container_by_mark(const char* mark);
GWMRect container_minimum_size(GWMContainer* con);
bool container_has_urgent_child(GWMContainer* con);
void container_activate_unblock(GWMContainer* con);
GWMContainer* container_by_container_id(long target);
bool container_has_managed_window(GWMContainer* con);
void container_disable_full_screen(GWMContainer* con);
int container_num_visible_children(GWMContainer* con);
char* container_parse_title_format(GWMContainer* con);
GWMContainer* container_get_output(GWMContainer* con);
GWMRect container_border_style_rect(GWMContainer* con);
GWMContainer* container_by_frame_id(xcb_window_t frame);
GWMOrientation container_orientation(GWMContainer* con);
GWMContainer* container_next_focused(GWMContainer* con);
GWMContainer* container_get_workspace(GWMContainer* con);
void container_update_parents_urgency(GWMContainer* con);
GWMContainer* container_by_window_id(xcb_window_t window);
GWMAdjacent container_adjacent_borders(GWMContainer* con);
void container_set_urgency(GWMContainer* con, bool urgent);
char *container_get_tree_representation(GWMContainer* con);
bool container_inside_stacked_or_tabbed(GWMContainer* con);
GWMContainer* container_descend_focused(GWMContainer* con);
void container_unmark(GWMContainer* con, const char* name);
GWMContainer* container_inside_floating(GWMContainer* con);
GWMContainer* container_descend_focused (GWMContainer* con);
GWMContainer** container_get_focus_order(GWMContainer* con);
void container_force_split_parents_redraw(GWMContainer* con);
bool container_draw_decoration_into_frame(GWMContainer* con);
bool container_has_mark(GWMContainer* con, const char* mark);
void container_set_layout(GWMContainer*con, GWMLayout layout);
bool container_swap(GWMContainer* first, GWMContainer* second);
uint32_t container_rect_size_in_orientation(GWMContainer* con);
void container_merge_into(GWMContainer*old, GWMContainer* new);
bool container_full_screen_permits_focusing(GWMContainer* con);
bool container_move_to_mark(GWMContainer* con, const char* mark);
void container_close(GWMContainer* con, GWMKillWindow killWindow);
GWMContainer* container_descend_tiling_focused(GWMContainer* con);
bool container_has_parent(GWMContainer* con, GWMContainer* parent);
GWMContainer* container_new(GWMContainer* parent, GWMWindow* window);
GWMContainer* container_get_full_screen_covering_ws(GWMContainer* ws);
bool container_move_to_target(GWMContainer* con, GWMContainer* target);
void container_toggle_layout(GWMContainer* con, const char* toggleMode);
void container_toggle_full_screen(GWMContainer* con, int fullScreenMode);
void container_mark(GWMContainer* con, const char* mark, GWMMarkMode mode);
void container_set_focus_order(GWMContainer* con, GWMContainer** focusOrder);
GWMContainer* container_new_skeleton(GWMContainer* parent, GWMWindow* window);
void container_attach(GWMContainer* con, GWMContainer*parent, bool ignoreFocus);
void container_mark_toggle(GWMContainer* con, const char* mark, GWMMarkMode mode);
bool container_find_transient_for_window(GWMContainer* start, xcb_window_t target);
GWMContainer* container_descend_direction(GWMContainer* con, GWMDirection direction);
void container_enable_full_screen(GWMContainer* con, GWMFullScreenMode fullScreenMode);
void container_move_to_output(GWMContainer* con, GWMOutput* output, bool fixCoordinates);
GWMContainer* container_for_window(GWMContainer* con, GWMWindow* window, Match** storeMatch);
bool container_move_to_output_name(GWMContainer* con, const char* name, bool fixCoordinates);
GWMContainer* container_parent_with_orientation(GWMContainer* con, GWMOrientation orientation);
void container_set_border_style(GWMContainer* con, GWMBorderStyle borderStyle, int borderWidth);
GWMContainer* container_get_full_screen_con(GWMContainer* con, GWMFullScreenMode fullScreenMode);
void container_move_to_workspace(GWMContainer*con, GWMContainer* workspace, bool fixCoordinates, bool doNotWarp, bool ignoreFocus);


#endif //GRACEFUL_WM_CONTAINER_H
