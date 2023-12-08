//
// Created by dingjing on 23-11-28.
//
#include "workspace.h"

#include "val.h"
#include "log.h"
#include "output.h"
#include "utils.h"
#include "randr.h"

void workspace_update_urgent_flag(GWMContainer *ws)
{

}

GWMContainer* workspace_encapsulate(GWMContainer* ws)
{
    return NULL;
}

bool workspace_is_visible(GWMContainer *ws)
{
    return 0;
}

GWMContainer *workspace_next(void)
{
    return NULL;
}

GWMContainer *workspace_prev(void)
{
    return NULL;
}

void workspace_back_and_forth(void)
{

}

void workspace_show(GWMContainer *ws)
{

}

GWMContainer *workspace_get(const char *num)
{
    return NULL;
}

void workspace_show_by_name(const char *num)
{

}

void workspace_extract_workspace_names_from_bindings(void)
{

}

GWMContainer *workspace_next_on_output(void)
{
    return NULL;
}

GWMContainer *workspace_prev_on_output(void)
{
    return NULL;
}

GWMContainer *workspace_back_and_forth_get(void)
{
    return NULL;
}

GWMContainer *workspace_attach_to(GWMContainer *ws)
{
    return NULL;
}

GWMContainer *workspace_get_existing_workspace_by_num(int num)
{
    GWMContainer* output, *workspace = NULL;
    TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
        GREP_FIRST(workspace, output_get_content(output), child->workspaceNum == num);
    }

    return workspace;
}

void workspace_move_to_output(GWMContainer *ws, GWMOutput *output)
{

}
GWMContainer* workspace_get_existing_workspace_by_name(const char *name)
{
    GWMContainer* output, *workspace = NULL;
    TAILQ_FOREACH (output, &(gContainerRoot->nodesHead), nodes) {
        GREP_FIRST(workspace, output_get_content(output), !strcasecmp(child->name, name));
    }

    return workspace;
}

GWMContainer *workspace_get_assigned_output(const char *name, long parsed_num)
{
    GWMContainer* output = NULL;
    GWMWorkspaceAssignment *assignment;
    TAILQ_FOREACH (assignment, &gWorkspaceAssignments, wsAssignments) {
        if (assignment->output == NULL) {
            continue;
        }

        if (name && strcmp(assignment->name, name) == 0) {
            DEBUG ("Found workspace name=\"%s\" assignment to output \"%s\"\n", name, assignment->output);
            GWMOutput *assigned_by_name = randr_get_output_by_name(assignment->output, true);
            if (assigned_by_name) {
                return assigned_by_name->container;
            }
        }
        else if (!output && parsed_num != -1 && util_name_is_digits(assignment->name) && util_ws_name_to_number(assignment->name) == parsed_num) {
            DEBUG("Found workspace number=%ld assignment to output \"%s\"", parsed_num, assignment->output);
            GWMOutput *assigned_by_num = randr_get_output_by_name(assignment->output, true);
            if (assigned_by_num) {
                output = assigned_by_num->container;
            }
        }
    }

    return output;
}

void workspace_ws_force_orientation(GWMContainer *ws, GWMOrientation orientation)
{

}

GWMContainer *workspace_create_workspace_on_output(GWMOutput *output, GWMContainer *content)
{
    return NULL;
}

static void _workspace_apply_default_orientation(GWMContainer* ws)
{
    ws->layout = L_SPLIT_H; //(config.default_orientation == HORIZ) ? L_SPLITH : L_SPLITV;
//    if (config.default_orientation == NO_ORIENTATION) {
//        Con *output = con_get_output(ws);
//        ws->layout = (output->rect.height > output->rect.width) ? L_SPLITV : L_SPLITH;
//        ws->rect = output->rect;
//        DLOG("Auto orientation. Workspace size set to (%d,%d), setting layout to %d.\n",
//             output->rect.width, output->rect.height, ws->layout);
//    } else {
//        ws->layout = (config.default_orientation == HORIZ) ? L_SPLITH : L_SPLITV;
//    }
}