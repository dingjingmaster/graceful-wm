//
// Created by dingjing on 23-11-24.
//

#include "randr.h"

GWMOutput *randr_get_output_containing(unsigned int x, unsigned int y)
{
    return NULL;
}

GWMOutput *randr_get_first_output(void)
{
    return NULL;
}

void randr_init(int *eventBase, bool disableRandr15)
{

}

GWMOutput *randr_get_output_next(GWMDirection direction, GWMOutput *current, GWMOutputCloseFar closeFar)
{
    return NULL;
}

GWMOutput *randr_get_output_next_wrap(GWMDirection direction, GWMOutput *current)
{
    return NULL;
}

GWMOutput *randr_get_output_by_name(const char *name, bool requireActive)
{
    return NULL;
}

GWMOutput *randr_create_root_output(xcb_connection_t *conn) {
    return NULL;
}

GWMOutput *randr_get_output_with_dimensions(GWMRect rect) {
    return NULL;
}

GWMOutput *randr_output_containing_rect(GWMRect rect) {
    return NULL;
}

GWMOutput *randr_get_output_from_rect(GWMRect rect) {
    return NULL;
}

void randr_output_init_container(GWMOutput *output) {

}

void randr_init_ws_for_output(GWMOutput *output) {

}

void randr_disable_output(GWMOutput *output) {

}

void randr_query_outputs(void) {

}
