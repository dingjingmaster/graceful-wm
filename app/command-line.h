//
// Created by dingjing on 23-11-23.
//

#ifndef GRACEFUL_WM_COMMAND_LINE_H
#define GRACEFUL_WM_COMMAND_LINE_H
#include <stdlib.h>
#include <stdbool.h>

#include "types.h"


typedef struct WMCommandLine WMCommandLine;

bool command_line_parse(int argc, int** argv);
void command_line_help();

bool command_line_get_is_replace();
bool command_line_get_is_only_check_config();

const char* command_line_get_config_path();

GWMConfigLoad command_line_get_load_type();

#endif //GRACEFUL_WM_COMMAND_LINE_H
