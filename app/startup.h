//
// Created by dingjing on 23-11-27.
//

#ifndef GRACEFUL_WM_STARTUP_H
#define GRACEFUL_WM_STARTUP_H
#include <libsn/sn-monitor.h>

#include "types.h"


void startup_monitor_event                  (SnMonitorEvent* event, void* udata);
void startup_sequence_delete                (GWMStartupSequence* sequence);
void startup_start_application              (const char *command, bool noStartupID);
void startup_sequence_rename_workspace      (const char *old_name, const char *new_name);
GWMStartupSequence* startup_sequence_get    (GWMWindow* cWindow, xcb_get_property_reply_t *startupIdReply, bool ignoreMappedLeader);
char* startup_workspace_for_window          (GWMWindow* cWindow, xcb_get_property_reply_t *startupIdReply);
void startup_sequence_delete_by_window      (GWMWindow* win);


#endif //GRACEFUL_WM_STARTUP_H
