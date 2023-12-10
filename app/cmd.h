//
// Created by dingjing on 23-12-10.
//

#ifndef GRACEFUL_WM_CMD_H
#define GRACEFUL_WM_CMD_H
#include "types.h"


char*               cmd_parse_string(const char **walk, bool as_word);
GWMCommandResult*   cmd_parse_command(const char *input, void* gen, void* client);
void                cmd_command_result_free(GWMCommandResult *result);

#endif //GRACEFUL_WM_CMD_H
