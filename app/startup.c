//
// Created by dingjing on 23-11-27.
//

#include "startup.h"

#include <glib/gi18n.h>

#include "log.h"
#include "types.h"
#include "cursor.h"


static int _prune_startup_sequences(void);



static GQueue* gStartupSequences = G_QUEUE_INIT;

void startup_monitor_event(SnMonitorEvent *event, void *udata)
{
    SnStartupSequence* snSequence = sn_monitor_event_get_startup_sequence (event);

    const char* id = sn_startup_sequence_get_id (snSequence);
    GWMStartupSequence* current = NULL;
    GWMStartupSequence* sequence = NULL;

    for (GList* ls = gStartupSequences->head; ls; ls = ls->next) {
        current = (GWMStartupSequence*) ls->data;
        if (0 == g_strcmp0 (id, current->id)) {
            sequence = current;
            break;
        }
    }

    if (!sequence) {
        DEBUG(_("Got event for startup sequence that we did not initiate (ID = $s). Ignoring."), id);
        return;
    }

    switch (sn_monitor_event_get_type (event)) {
        case SN_MONITOR_EVENT_COMPLETED: {
            DEBUG(_("Startup sequence %s completed"), id);
            time_t curTime = time(NULL);
            sequence->deleteAt = curTime + 30;
            DEBUG(_("Will delete startup sequence %s at timestamp %lld"), sequence->id, (long long) sequence->deleteAt);
            if (0 == _prune_startup_sequences()) {
                DEBUG(_("No more startup sequences running, changing root window cursor to default pointer."));
                cursor_set_root_cursor (CURSOR_POINTER);
            }
            break;
        }
        default: {
            // ignore
            break;
        }
    }
}

void startup_sequence_delete(GWMStartupSequence* sequence)
{
    g_return_if_fail(sequence != NULL);
    DEBUG(_("Deleting startup sequence %s, delete_at = %lld, current_time = %lld"),
         sequence->id, (long long)sequence->deleteAt, (long long) time (NULL));

    /* Unref the context, will be free()d */
    sn_launcher_context_unref(sequence->context);

    /* Delete our internal sequence */
    GList* del = NULL;
    for (GList* ls = gStartupSequences->head; ls ; ls = ls->next) {
        if (0 == g_strcmp0 (sequence->id, ((GWMStartupSequence*)ls->data)->id)) {
            del = ls;
            break;
        }
    }
    g_return_if_fail(del);

    g_queue_remove (gStartupSequences, del);

    free(sequence->id);
    free(sequence->workspace);
    {
        free (sequence);
        sequence = NULL;
    }
}

void startup_start_application(const char *command, bool noStartupID)
{

}

void startup_sequence_rename_workspace(const char *old_name, const char *new_name)
{

}

GWMStartupSequence* startup_sequence_get(GWMWindow *cWindow, xcb_get_property_reply_t *startupIdReply, bool ignoreMappedLeader)
{
    return NULL;
}

char *startup_workspace_for_window(GWMWindow *cWindow, xcb_get_property_reply_t *startupIdReply)
{
    return NULL;
}

void startup_sequence_delete_by_window(GWMWindow *win)
{

}

static int _prune_startup_sequences(void)
{
    time_t curTime = time(NULL);
    int activeSequences = 0;

    GWMStartupSequence *current, *next;

    for (GList* ls = gStartupSequences->head; ls; ls = ls->next) {
        current = ls->data;
        if (0 == current->deleteAt) {
            ++activeSequences;
            continue;
        }

        if (curTime <= current->deleteAt) {
            continue;
        }

        startup_sequence_delete(current);
    }

    return activeSequences;
}