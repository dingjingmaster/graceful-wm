//
// Created by dingjing on 23-12-8.
//

#ifndef GRACEFUL_WM_MATCH_H
#define GRACEFUL_WM_MATCH_H
#include "types.h"

void match_init(GWMMatch *match);

bool match_is_empty(GWMMatch *match);

void match_copy(GWMMatch *dest, GWMMatch *src);

bool match_matches_window(GWMMatch *match, GWMWindow *window);

void match_free(GWMMatch *match);

void match_parse_property(GWMMatch *match, const char *cType, const char *cValue);


#endif //GRACEFUL_WM_MATCH_H
