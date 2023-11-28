//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_EXTEND_WM_HINTS_H
#define GRACEFUL_WM_EXTEND_WM_HINTS_H
#include "types.h"


/**
 * @brief Extended Window Manager Hints
 */
void extend_wm_hint_setup_hint                      (void);
void extend_wm_hint_update_desktop_properties       (void);
void extend_wm_hint_update_current_desktop          (void);
void extend_wm_hint_update_wm_desktop               (void);
void extend_wm_hint_update_work_area                (void);
void extend_wm_hint_update_active_window            (xcb_window_t window);
void extend_wm_hint_update_sticky                   (xcb_window_t window, bool sticky);
void extend_wm_hint_update_client_list              (xcb_window_t* list, int numWindows);
void extend_wm_hint_update_focused                  (xcb_window_t window, bool isFocused);
void extend_wm_hint_update_client_list_stacking     (xcb_window_t* stack, int numWindows);
void extend_wm_hint_update_visible_name             (xcb_window_t window, const char* name);

GWMContainer* extend_wm_hint_get_workspace_by_index (uint32_t idx);
uint32_t extend_wm_hint_get_workspace_index         (GWMContainer* con);


#endif //GRACEFUL_WM_EXTEND_WM_HINTS_H
