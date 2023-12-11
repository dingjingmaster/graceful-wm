//
// Created by dingjing on 23-11-24.
//

#include "output.h"

#include "val.h"
#include "log.h"
#include "types.h"
#include "randr.h"
#include "container.h"
#include "workspace.h"


GWMContainer *output_get_content(GWMContainer *output)
{
    g_return_val_if_fail(output, NULL);

    GWMContainer* child = NULL;
    TAILQ_FOREACH (child, &(output->nodesHead), nodes) {
        if (child->type == CT_CON) {
            return child;
        }
    }

    return NULL;
}

char *output_primary_name(GWMOutput *output)
{
    return SLIST_FIRST(&output->namesHead)->name;
}

void output_push_sticky_windows(GWMContainer *oldFocus)
{
    GWMContainer* output;
    TAILQ_FOREACH (output, &(gContainerRoot->focusHead), focused) {
        GWMContainer* workspace, *visible_ws = NULL;
        GREP_FIRST(visible_ws, output_get_content(output), workspace_is_visible(child));

        for (workspace = TAILQ_FIRST(&(output_get_content(output)->focusHead));
                workspace != TAILQ_END(&(output_get_content(output)->focusHead));) {
            GWMContainer* current_ws = workspace;
            workspace = TAILQ_NEXT(workspace, focused);

            GWMContainer* child;
            for (child = TAILQ_FIRST(&(current_ws->focusHead)); child != TAILQ_END(&(current_ws->focusHead));) {
                GWMContainer* current = child;
                child = TAILQ_NEXT(child, focused);
                if (current->type != CT_FLOATING_CON || !container_is_sticky(current)) {
                    continue;
                }

                bool ignore_focus = (oldFocus == NULL) || (current != oldFocus->parent);
                container_move_to_workspace(current, visible_ws, true, false, ignore_focus);
                if (!ignore_focus) {
                    GWMContainer* current_ws = container_get_workspace(gFocused);
                    container_activate(container_descend_focused(current));
                    container_activate(container_descend_focused(current_ws));
                }
            }
        }
    }
}

GWMOutput *output_get_output_for_con(GWMContainer *con)
{
    GWMContainer* outputCon = container_get_output(con);
    GWMOutput *output = randr_get_output_by_name(outputCon->name, true);
    g_assert(output != NULL);

    return output;
}

GWMOutput *output_get_output_from_string(GWMOutput *currentOutput, const char *outputStr)
{
    if (strcasecmp(outputStr, "current") == 0) {
        return output_get_output_for_con(gFocused);
    }
    else if (strcasecmp(outputStr, "left") == 0) {
        return randr_get_output_next_wrap(D_LEFT, currentOutput);
    }
    else if (strcasecmp(outputStr, "right") == 0) {
        return randr_get_output_next_wrap(D_RIGHT, currentOutput);
    }
    else if (strcasecmp(outputStr, "up") == 0) {
        return randr_get_output_next_wrap(D_UP, currentOutput);
    }
    else if (strcasecmp(outputStr, "down") == 0) {
        return randr_get_output_next_wrap(D_DOWN, currentOutput);
    }

    return randr_get_output_by_name(outputStr, true);
}
