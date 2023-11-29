//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_OUTPUT_H
#define GRACEFUL_WM_OUTPUT_H
#include "types.h"

char* output_primary_name(GWMOutput *output);
GWMContainer* output_get_content (GWMContainer* output);
void output_push_sticky_windows(GWMContainer* oldFocus);
GWMOutput* output_get_output_for_con(GWMContainer* con);
GWMOutput* output_get_output_from_string(GWMOutput* currentOutput, const char* outputStr);

#endif //GRACEFUL_WM_OUTPUT_H
