//
// Created by dingjing on 23-11-23.
//

#ifndef GRACEFUL_WM_COMMAND_LINE_H
#define GRACEFUL_WM_COMMAND_LINE_H
#include <stdlib.h>
#include <stdbool.h>

typedef struct WMCommandLine WMCommandLine;

bool command_line_parse(int argc, int** argv);
void command_line_help();

bool command_line_get_is_replace();
bool command_line_get_is_only_check_config();

#endif //GRACEFUL_WM_COMMAND_LINE_H
