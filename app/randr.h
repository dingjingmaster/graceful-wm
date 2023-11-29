//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_RANDR_H
#define GRACEFUL_WM_RANDR_H
#include "types.h"

void randr_query_outputs(void);
GWMOutput* randr_get_first_output (void);
void randr_disable_output(GWMOutput *output);
void randr_init_ws_for_output(GWMOutput *output);
void randr_output_init_container(GWMOutput *output);
GWMOutput* randr_get_output_from_rect(GWMRect rect);
void randr_init(int *eventBase, bool disableRandr15);
GWMOutput* randr_output_containing_rect(GWMRect rect);
GWMOutput* randr_get_output_with_dimensions(GWMRect rect);
GWMOutput* randr_create_root_output(xcb_connection_t *conn);
GWMOutput* randr_get_output_containing (unsigned int x, unsigned int y);
GWMOutput* randr_get_output_by_name(const char *name, bool requireActive);
GWMOutput* randr_get_output_next_wrap(GWMDirection direction, GWMOutput* current);
GWMOutput* randr_get_output_next(GWMDirection direction, GWMOutput* current, GWMOutputCloseFar closeFar);

#endif //GRACEFUL_WM_RANDR_H
