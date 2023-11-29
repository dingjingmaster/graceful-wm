//
// Created by dingjing on 23-11-28.
//

#include "workspace.h"

void workspace_update_urgent_flag(GWMContainer *ws)
{

}

GWMContainer* workspace_encapsulate(GWMContainer* ws)
{
//    if (TAILQ_EMPTY(&(ws->nodes_head))) {
//        ELOG("Workspace %p / %s has no children to encapsulate\n", ws, ws->name);
//        return NULL;
//    }
//
//    Con *new = con_new(NULL, NULL);
//    new->parent = ws;
//    new->layout = ws->layout;
//
//    Con **focus_order = get_focus_order(ws);
//
//    DLOG("Moving children of workspace %p / %s into container %p\n",
//         ws, ws->name, new);
//    Con *child;
//    while (!TAILQ_EMPTY(&(ws->nodes_head))) {
//        child = TAILQ_FIRST(&(ws->nodes_head));
//        con_detach(child);
//        con_attach(child, new, true);
//    }
//
//    set_focus_order(new, focus_order);
//    free(focus_order);
//
//    con_attach(new, ws, true);
//
//    return new;
    return NULL;
}

bool workspace_is_visible(GWMContainer *ws)
{
    return 0;
}
