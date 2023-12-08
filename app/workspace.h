//
// Created by dingjing on 23-11-28.
//

#ifndef GRACEFUL_WM_workspace_H
#define GRACEFUL_WM_workspace_H
#include "types.h"


#define NET_WM_DESKTOP_NONE             0xFFFFFFF0
#define NET_WM_DESKTOP_ALL              0xFFFFFFFF


GWMContainer* workspace_next(void);
GWMContainer* workspace_prev(void);
void workspace_back_and_forth(void);
void workspace_show(GWMContainer* ws);
bool workspace_is_visible(GWMContainer* ws);
GWMContainer* workspace_get(const char *num);
void workspace_show_by_name(const char *num);
GWMContainer* workspace_next_on_output(void);
GWMContainer* workspace_prev_on_output(void);
GWMContainer* workspace_back_and_forth_get(void);
void workspace_extract_workspace_names_from_bindings(void);
GWMContainer* workspace_attach_to(GWMContainer* ws);
void workspace_update_urgent_flag(GWMContainer* ws);
GWMContainer* workspace_encapsulate(GWMContainer* ws);
GWMContainer* workspace_get_existing_workspace_by_num(int num);
void workspace_move_to_output(GWMContainer* ws, GWMOutput *output);
GWMContainer* workspace_get_existing_workspace_by_name(const char *name);
GWMContainer* workspace_get_assigned_output(const char *name, long parsed_num);
void workspace_ws_force_orientation(GWMContainer* ws, GWMOrientation orientation);
GWMContainer* workspace_create_workspace_on_output(GWMOutput* output, GWMContainer* content);
bool workspace_output_triggers_assignment(GWMOutput *output, struct workspace_Assignment *assignment);


#endif //GRACEFUL_WM_workspace_H
