//
// Created by dingjing on 23-12-10.
//

#ifndef GRACEFUL_WM_DRAG_H
#define GRACEFUL_WM_DRAG_H
#include "types.h"


#define DRAGGING_CB(name) \
    static void name(GWMContainer* con, GWMRect* oldRect, uint32_t new_x, uint32_t new_y, const xcb_button_press_event_t *event, const void *extra)


typedef void (*CallbackCB)(GWMContainer*, GWMRect*, uint32_t, uint32_t, const xcb_button_press_event_t*, const void*);

GWMDragResult drag_pointer(GWMContainer* con, const xcb_button_press_event_t *event, xcb_window_t confine_to, int cursor, bool useThreshold, CallbackCB callback, const void *extra);


// tiling drag
bool tiling_drag_has_drop_targets(void);
void tiling_drag(GWMContainer* con, xcb_button_press_event_t *event, bool useThreshold);


#endif //GRACEFUL_WM_DRAG_H
