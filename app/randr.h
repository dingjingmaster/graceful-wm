//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_RANDR_H
#define GRACEFUL_WM_RANDR_H
#include "types.h"

GWMOutput* randr_get_first_output (void);
GWMOutput* randr_get_output_containing (unsigned int x, unsigned int y);

#endif //GRACEFUL_WM_RANDR_H
