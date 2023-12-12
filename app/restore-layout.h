//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_RESTORE_LAYOUT_H
#define GRACEFUL_WM_RESTORE_LAYOUT_H
#include "types.h"


void restore_connect(void);
bool restore_kill_placeholder(xcb_window_t placeholder);
void restore_open_placeholder_windows(GWMContainer* parent);

#endif //GRACEFUL_WM_RESTORE_LAYOUT_H
