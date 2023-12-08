//
// Created by dingjing on 23-12-8.
//

#ifndef GRACEFUL_WM_REGEX_H
#define GRACEFUL_WM_REGEX_H
#include "types.h"


void regex_free(GWMRegex* regex);
GWMRegex* regex_new(const char *pattern);
bool regex_matches(GWMRegex* regex, const char *input);


#endif //GRACEFUL_WM_REGEX_H
