//
// Created by dingjing on 23-11-27.
//

#include "startup.h"

#include <paths.h>
#include <unistd.h>
#include <glib/gi18n.h>

#include "log.h"
#include "val.h"
#include "types.h"
#include "cursor.h"
#include "container.h"


static TAILQ_HEAD(startupsequenceHead, StartupSequence) gsStartupSequences = TAILQ_HEAD_INITIALIZER(gsStartupSequences);


static int _prune_startup_sequences(void);
static void startup_timeout(EV_P_ ev_timer *w, int revents);



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
    DEBUG(_ ("Deleting startup sequence %s, delete_at = %lld, current_time = %lld"), sequence->id,
            (long long) sequence->deleteAt, (long long) time (NULL));

    /* Unref the context, will be FREE()d */
    sn_launcher_context_unref (sequence->context);

    /* Delete our internal sequence */
    TAILQ_REMOVE(&gsStartupSequences, sequence, sequences);

    free (sequence->id);
    free (sequence->workspace);
    FREE(sequence);
}

void startup_start_application(const char *command, bool noStartupID)
{
    SnLauncherContext *context = NULL;

    if (!noStartupID) {
        context = sn_launcher_context_new(gSnDisplay, gConnScreen);
        sn_launcher_context_set_name(context, "gwm");
        sn_launcher_context_set_description(context, "exec command in i3");
        char *first_word = g_strdup(command);
        char *space = strchr(first_word, ' ');
        if (space) {
            *space = '\0';
        }
        sn_launcher_context_initiate(context, "gwm", first_word, gLastTimestamp);
        FREE(first_word);

        /* Trigger a timeout after 60 seconds */
        struct ev_timer *timeout = g_malloc(sizeof(struct ev_timer));
        EXIT_IF_MEM_IS_NULL(timeout);
        ev_timer_init(timeout, startup_timeout, 60.0, 0.);
        timeout->data = context;
        ev_timer_start(gMainLoop, timeout);

        DEBUG("startup id = %s", sn_launcher_context_get_startup_id(context));

        GWMContainer* ws = container_get_workspace(gFocused);
        GWMStartupSequence* sequence = g_malloc0(sizeof(GWMStartupSequence));
        sequence->id = g_strdup(sn_launcher_context_get_startup_id(context));
        sequence->workspace = g_strdup(ws->name);
        sequence->context = context;
        TAILQ_INSERT_TAIL(&gsStartupSequences, sequence, sequences);

        sn_launcher_context_ref(context);
    }

    DEBUG("executing: %s", command);
    if (fork() == 0) {
        setsid();
        setrlimit(RLIMIT_CORE, &gOriginalRLimitCore);
        for (int fd = SD_LISTEN_FDS_START; fd < (SD_LISTEN_FDS_START + gListenFds); fd++) {
            close(fd);
        }
        unsetenv("LISTEN_PID");
        unsetenv("LISTEN_FDS");
        signal(SIGPIPE, SIG_DFL);
        if (!noStartupID) {
            sn_launcher_context_setup_child_process(context);
        }
        setenv("GWM-SOCK", gCurrentSocketPath, 1);

        execl(_PATH_BSHELL, _PATH_BSHELL, "-c", command, NULL);
    }

    if (!noStartupID) {
        cursor_set_root_cursor(CURSOR_WATCH);
    }
}

void startup_sequence_rename_workspace(const char *old_name, const char *new_name)
{
    GWMStartupSequence* current;
    TAILQ_FOREACH (current, &gsStartupSequences, sequences) {
        if (strcmp(current->workspace, old_name) != 0)
            continue;
        DEBUG("Renaming workspace \"%s\" to \"%s\" in startup sequence %s.", old_name, new_name, current->id);
        FREE(current->workspace);
        current->workspace = g_strdup(new_name);
    }
}

GWMStartupSequence* startup_sequence_get(GWMWindow *cWindow, xcb_get_property_reply_t *startupIdReply, bool ignoreMappedLeader)
{
    if (startupIdReply == NULL || xcb_get_property_value_length(startupIdReply) == 0) {
        FREE(startupIdReply);
        DEBUG("No _NET_STARTUP_ID set on window 0x%08x", cWindow->id);
        if (cWindow->leader == XCB_NONE) {
            return NULL;
        }

        if (ignoreMappedLeader && container_by_window_id(cWindow->leader) != NULL) {
            DEBUG("Ignoring leader window 0x%08x", cWindow->leader);
            return NULL;
        }

        DEBUG("Checking leader window 0x%08x", cWindow->leader);

        xcb_get_property_cookie_t cookie;

        cookie = xcb_get_property(gConn, false, cWindow->leader, A__NET_STARTUP_ID, XCB_GET_PROPERTY_TYPE_ANY, 0, 512);
        startupIdReply = xcb_get_property_reply(gConn, cookie, NULL);

        if (startupIdReply == NULL || xcb_get_property_value_length(startupIdReply) == 0) {
            FREE(startupIdReply);
            DEBUG("No _NET_STARTUP_ID set on the leader either");
            return NULL;
        }
    }

    char *startup_id = g_strdup_printf("%.*s", xcb_get_property_value_length(startupIdReply), (char *)xcb_get_property_value(startupIdReply));
    struct StartupSequence *current, *sequence = NULL;
    TAILQ_FOREACH (current, &gsStartupSequences, sequences) {
        if (strcmp(current->id, startup_id) != 0) {
            continue;
        }
        sequence = current;
        break;
    }

    if (!sequence) {
        DEBUG("WARNING: This sequence (ID %s) was not found", startup_id);
        FREE(startup_id);
        FREE(startupIdReply);
        return NULL;
    }

    FREE(startup_id);
    FREE(startupIdReply);

    return sequence;
}

char *startup_workspace_for_window(GWMWindow *cWindow, xcb_get_property_reply_t *startupIdReply)
{
    struct StartupSequence *sequence = startup_sequence_get(cWindow, startupIdReply, false);
    if (sequence == NULL) {
        return NULL;
    }

    /* If the startup sequence's time span has elapsed, delete it. */
    time_t current_time = time(NULL);
    if (sequence->deleteAt > 0 && current_time > sequence->deleteAt) {
        DEBUG("Deleting expired startup sequence %s", sequence->id);
        startup_sequence_delete(sequence);
        return NULL;
    }

    return sequence->workspace;
}

void startup_sequence_delete_by_window(GWMWindow *win)
{
    struct StartupSequence *sequence;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *startup_id_reply;

    cookie = xcb_get_property(gConn, false, win->id, A__NET_STARTUP_ID, XCB_GET_PROPERTY_TYPE_ANY, 0, 512);
    startup_id_reply = xcb_get_property_reply(gConn, cookie, NULL);

    sequence = startup_sequence_get(win, startup_id_reply, true);
    if (sequence != NULL) {
        startup_sequence_delete(sequence);
    }
}

static int _prune_startup_sequences(void)
{
    time_t curTime = time(NULL);
    int activeSequences = 0;

    GWMStartupSequence *current, *next;

    for (next = TAILQ_FIRST(&gsStartupSequences); next != TAILQ_END(&gsStartupSequences);) {
        current = next;
        next = TAILQ_NEXT(next, sequences);
        if (current->deleteAt == 0) {
            activeSequences++;
            continue;
        }
        if (curTime <= current->deleteAt) {
            continue;
        }
        startup_sequence_delete(current);
    }

    return activeSequences;
}

static void startup_timeout(EV_P_ ev_timer *w, int revents)
{
    const char *id = sn_launcher_context_get_startup_id(w->data);
    DEBUG("Timeout for startup sequence %s", id);

    struct StartupSequence* current, *sequence = NULL;
    TAILQ_FOREACH (current, &gsStartupSequences, sequences) {
        if (strcmp(current->id, id) != 0) {
            continue;
        }
        sequence = current;
        break;
    }

    sn_launcher_context_unref(w->data);

    if (!sequence) {
        DEBUG("Sequence already deleted, nevermind.");
        FREE(w);
        return;
    }

    sn_launcher_context_complete(w->data);
    FREE(w);
}