//
// Created by dingjing on 23-12-10.
//

#ifndef GRACEFUL_WM_RESIZE_H
#define GRACEFUL_WM_RESIZE_H
#include "types.h"


double resize_percent_for_1px(GWMContainer* con);

bool resize_neighboring_cons(GWMContainer* first, GWMContainer* second, int px, int ppt);

bool resize_find_tiling_participants(GWMContainer** current, GWMContainer** other, GWMDirection direction, bool bothSides);

void resize_graphical_handler(GWMContainer* first, GWMContainer* second, GWMOrientation orientation, const xcb_button_press_event_t* event, bool useThreshold);

#endif //GRACEFUL_WM_RESIZE_H
