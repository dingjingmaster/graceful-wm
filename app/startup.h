//
// Created by dingjing on 23-11-27.
//

#ifndef GRACEFUL_WM_STARTUP_H
#define GRACEFUL_WM_STARTUP_H
#include <libsn/sn-monitor.h>

#include "types.h"


void startup_monitor_event      (SnMonitorEvent* event, void* udata);
void startup_sequence_delete    (GWMStartupSequence* sequence);



#endif //GRACEFUL_WM_STARTUP_H
