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


GWMContainer* container_descend_focused (GWMContainer* con);
void container_activate (GWMContainer* con);


#endif //GRACEFUL_WM_CONTAINER_H
