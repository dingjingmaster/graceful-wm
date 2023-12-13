//
// Created by dingjing on 23-11-28.
//

#ifndef GRACEFUL_WM_CONFIG_H
#define GRACEFUL_WM_CONFIG_H

#include "types.h"

GWMEventStateMask config_event_state_from_str (const char* str);

void config_start_config_error_nag_bar(const char* configPath, bool has_errors);

bool config_load_configuration(const char *overrideConfigfile);
void config_ungrab_all_keys(xcb_connection_t *conn);

#endif //GRACEFUL_WM_CONFIG_H
