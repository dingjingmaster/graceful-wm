//
// Created by dingjing on 23-12-10.
//

#include "assignments.h"
#include "match.h"
#include "log.h"
#include "tree.h"
#include "val.h"
#include "cmd.h"

void assignments_run(GWMWindow *window)
{
    DEBUG("Checking if any assignments match this window\n");

    bool needs_tree_render = false;

    GWMAssignment *current;
    TAILQ_FOREACH (current, &gAssignments, assignments) {
        if (current->type != A_COMMAND || !match_matches_window(&(current->match), window)) {
            continue;
        }

        bool skip = false;
        for (uint32_t c = 0; c < window->nrAssignments; c++) {
            if (window->ranAssignments[c] != current) {
                continue;
            }

            DEBUG("This assignment already ran for the given window, not executing it again.\n");
            skip = true;
            break;
        }

        if (skip) {
            continue;
        }

        window->nrAssignments++;
        window->ranAssignments = realloc(window->ranAssignments, sizeof(GWMAssignment*) * window->nrAssignments);
        window->ranAssignments[window->nrAssignments - 1] = current;

        DEBUG("matching assignment, execute command %s\n", current->destination.command);
        char *full_command = g_strdup_printf("[id=\"%d\"] %s", window->id, current->destination.command);
        GWMCommandResult *result = cmd_parse_command(full_command, NULL, NULL);
        free(full_command);

        if (result->needsTreeRender) {
            needs_tree_render = true;
        }

        cmd_command_result_free(result);
    }

    /* If any of the commands required re-rendering, we will do that now. */
    if (needs_tree_render) {
        tree_render();
    }
}

GWMAssignment *assignment_for(GWMWindow *window, int type)
{
    GWMAssignment *assignment;

    TAILQ_FOREACH (assignment, &gAssignments, assignments) {
        if ((type != A_ANY && (assignment->type & type) == 0) || !match_matches_window(&(assignment->match), window)) {
            continue;
        }
        DEBUG("got a matching assignment\n");
        return assignment;
    }

    return NULL;
}
