//
// Created by dingjing on 23-11-24.
//

#include "output.h"

#include "val.h"
#include "log.h"
#include "types.h"
#include "randr.h"
#include "container.h"


GWMContainer *output_get_content(GWMContainer *output)
{
    g_return_val_if_fail(output, NULL);

    for (GList* ls = output->nodesHead.head; ls; ls = ls->next) {
        GWMContainer* child = (GWMContainer*) ls->data;
        if (CT_CON == child->type) {
            return child;
        }
    }

    return NULL;
}

char *output_primary_name(GWMOutput *output)
{
    return ((GWMOutputName*)(output->namesHead.head->data))->name;
}

void output_push_sticky_windows(GWMContainer *oldFocus)
{
    for (GList* ls = gContainerRoot->focusHead.head; ls; ls = ls->next) {
        GWMContainer* output = ls->data;
        GWMContainer* workspace, *visible_ws = NULL;
//        GREP_FIRST(visible_ws, output_get_content(output), workspace_is_visible(child));
//
//        /* We use this loop instead of TAILQ_FOREACH to avoid problems if the
//         * sticky window was the last window on that workspace as moving it in
//         * this case will close the workspace. */
//        for (workspace = TAILQ_FIRST(&(output_get_content(output)->focus_head));
//             workspace != TAILQ_END(&(output_get_content(output)->focus_head));) {
//            Con *current_ws = workspace;
//            workspace = TAILQ_NEXT(workspace, focused);
//
//            /* Since moving the windows actually removes them from the list of
//             * floating windows on this workspace, here too we need to use
//             * another loop than TAILQ_FOREACH. */
//            Con *child;
//            for (child = TAILQ_FIRST(&(current_ws->focus_head));
//                 child != TAILQ_END(&(current_ws->focus_head));) {
//                Con *current = child;
//                child = TAILQ_NEXT(child, focused);
//                if (current->type != CT_FLOATING_CON || !con_is_sticky(current)) {
//                    continue;
//                }
//
//                bool ignore_focus = (old_focus == NULL) || (current != old_focus->parent);
//                con_move_to_workspace(current, visible_ws, true, false, ignore_focus);
//                if (!ignore_focus) {
//                    Con *current_ws = con_get_workspace(focused);
//                    con_activate(con_descend_focused(current));
//                    /* Pushing sticky windows shouldn't change the focused workspace. */
//                    con_activate(con_descend_focused(current_ws));
//                }
//            }
//        }
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
