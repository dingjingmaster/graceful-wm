//
// Created by dingjing on 23-12-10.
//

#ifndef GRACEFUL_WM_GAPS_H
#define GRACEFUL_WM_GAPS_H
#include "types.h"


GWMGaps gaps_for_workspace(GWMContainer* ws);

void gaps_reapply_workspace_assignments(void);

GWMGaps gaps_calculate_effective_gaps(GWMContainer* con);

bool gaps_should_inset_con(GWMContainer* con, int children);

bool gaps_has_adjacent_container(GWMContainer* con, GWMDirection direction);


#endif //GRACEFUL_WM_GAPS_H
