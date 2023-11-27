//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_HANDLERS_H
#define GRACEFUL_WM_HANDLERS_H
#include <stdbool.h>

#include <xcb/xcb.h>
#include <xcb/randr.h>


void handler_property_init      ();
void handler_add_ignore_event   (int sequence, int responseType);
bool handler_event_is_ignored   (int sequence, int responseType);
void handler_handle_event       (int type, xcb_generic_event_t* event);

#endif //GRACEFUL_WM_HANDLERS_H
