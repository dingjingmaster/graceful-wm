//
// Created by dingjing on 23-11-24.
//
#include "restore-layout.h"

#include "dpi.h"
#include "val.h"
#include "log.h"
#include "draw-util.h"
#include "container.h"
#include "xcb.h"
#include "utils.h"
#include "match.h"


#define TEXT_PADDING dpi_logical_px(2)

typedef struct PlaceholderState GWMPlaceholderState;

struct PlaceholderState
{
    xcb_window_t                    window;
    GWMContainer*                   con;
    GWMRect                         rect;
    GWMSurface                      surface;
    TAILQ_ENTRY(PlaceholderState)   state;
};

static void expose_event(xcb_expose_event_t *event);
static void update_placeholder_contents(GWMPlaceholderState* state);
static void restore_xcb_prepare_cb(EV_P_ ev_prepare *w, int rEvents);
static void restore_xcb_got_event(EV_P_ struct ev_io *w, int rEvents);
static void restore_handle_event(int type, xcb_generic_event_t *event);


static xcb_connection_t*                            gsRestoreConn = NULL;
static struct ev_prepare*                           gsXCBPrepare = NULL;
static struct ev_io*                                gsXCBWatcher = NULL;
static TAILQ_HEAD(stateHead, PlaceholderState)      gsStateHead = TAILQ_HEAD_INITIALIZER(gsStateHead);


void restore_connect(void)
{
    if (gsRestoreConn != NULL) {
        ev_io_stop(gMainLoop, gsXCBWatcher);
        ev_prepare_stop(gMainLoop, gsXCBPrepare);

        GWMPlaceholderState *state;
        while (!TAILQ_EMPTY(&gsStateHead)) {
            state = TAILQ_FIRST(&gsStateHead);
            TAILQ_REMOVE(&gsStateHead, state, state);
            free(state);
        }

        xcb_disconnect(gsRestoreConn);
        free(gsXCBWatcher);
        free(gsXCBPrepare);
    }

    int screen;
    gsRestoreConn = xcb_connect(NULL, &screen);
    if (gsRestoreConn == NULL || xcb_connection_has_error(gsRestoreConn)) {
        if (gsRestoreConn != NULL) {
            xcb_disconnect(gsRestoreConn);
        }
        ERROR("Cannot open display");
        exit(-1);
    }

    gsXCBWatcher = g_malloc0(sizeof(struct ev_io));
    gsXCBPrepare = g_malloc0(sizeof(struct ev_prepare));

    ev_io_init(gsXCBWatcher, restore_xcb_got_event, xcb_get_file_descriptor(gsRestoreConn), EV_READ);
    ev_io_start(gMainLoop, gsXCBWatcher);

    ev_prepare_init(gsXCBPrepare, restore_xcb_prepare_cb);
    ev_prepare_start(gMainLoop, gsXCBPrepare);
}

void restore_open_placeholder_windows(GWMContainer* con)
{
    if (container_is_leaf(con)
        && (con->window == NULL || con->window->id == XCB_NONE)
        && !TAILQ_EMPTY(&(con->swallowHead))
        && con->type == CT_CON) {
        xcb_window_t placeholder = xcb_gwm_create_window(
            gsRestoreConn,
            con->rect,
            XCB_COPY_FROM_PARENT,
            XCB_COPY_FROM_PARENT,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            CURSOR_POINTER,
            true,
            XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
            (uint32_t[]){2,
                XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
            });
        xcb_icccm_wm_hints_t hints;
        xcb_icccm_wm_hints_set_none(&hints);
        xcb_icccm_wm_hints_set_input(&hints, 0);
        xcb_icccm_set_wm_hints(gsRestoreConn, placeholder, &hints);
        if (con->name != NULL) {
            xcb_change_property(gsRestoreConn, XCB_PROP_MODE_REPLACE, placeholder, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen(con->name), con->name);
        }
        DEBUG("Created placeholder window 0x%08x for leaf container %p / %s", placeholder, con, con->name);

        GWMPlaceholderState* state = g_malloc0(sizeof(GWMPlaceholderState));
        state->window = placeholder;
        state->con = con;
        state->rect = con->rect;

        draw_util_surface_init(gsRestoreConn, &(state->surface), placeholder, util_get_visual_type(gRootScreen), state->rect.width, state->rect.height);
        update_placeholder_contents(state);
        TAILQ_INSERT_TAIL(&gsStateHead, state, state);

        GWMMatch *temp_id = g_malloc0(sizeof(GWMMatch));
        match_init(temp_id);
        temp_id->dock = M_DO_NOT_CHECK;
        temp_id->id = placeholder;
        TAILQ_INSERT_HEAD(&(con->swallowHead), temp_id, matches);
    }

    GWMContainer* child;
    TAILQ_FOREACH (child, &(con->nodesHead), nodes) {
        restore_open_placeholder_windows(child);
    }
    TAILQ_FOREACH (child, &(con->floatingHead), floatingWindows) {
        restore_open_placeholder_windows(child);
    }
}

bool restore_kill_placeholder(xcb_window_t placeholder)
{
    return 0;
}

static void restore_xcb_got_event(EV_P_ struct ev_io *w, int rEvents)
{

}

static void restore_xcb_prepare_cb(EV_P_ ev_prepare *w, int rEvents)
{
    xcb_generic_event_t *event;

    if (xcb_connection_has_error(gsRestoreConn)) {
        DEBUG("restore X11 connection has an error, reconnecting");
        restore_connect();
        return;
    }

    while ((event = xcb_poll_for_event(gsRestoreConn)) != NULL) {
        if (event->response_type == 0) {
            xcb_generic_error_t *error = (xcb_generic_error_t *)event;
            DEBUG("X11 Error received (probably harmless)! sequence 0x%x, error_code = %d", error->sequence, error->error_code);
            free(event);
            continue;
        }

        int type = (event->response_type & 0x7F);

        restore_handle_event(type, event);

        free(event);
    }
    xcb_flush(gsRestoreConn);
}

static void restore_handle_event(int type, xcb_generic_event_t *event)
{
    switch (type) {
        case XCB_EXPOSE: {
            if (((xcb_expose_event_t *)event)->count == 0) {
                expose_event((xcb_expose_event_t *)event);
            }
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
//            configure_notify((xcb_configure_notify_event_t *)event);
            break;
        }
        default:
            DEBUG("Received unhandled X11 event of type %d", type);
            break;
    }
}

static void expose_event(xcb_expose_event_t *event)
{
    GWMPlaceholderState* state;
    TAILQ_FOREACH (state, &gsStateHead, state) {
        if (state->window != event->window) {
            continue;
        }

        DEBUG("refreshing window 0x%08x contents (con %p)", state->window, state->con);

        update_placeholder_contents(state);

        return;
    }

    ERROR("Received ExposeEvent for unknown window 0x%08x", event->window);
}

static void update_placeholder_contents(GWMPlaceholderState* state)
{
    const GWMColor foreground = {
        .red = 0,
        .green = 0,
        .blue = 0,
    };
    const GWMColor background = {
        .red = 255,
        .green = 255,
        .blue = 255,
    };

    draw_util_clear_surface(&(state->surface), background);

    xcb_aux_sync(gsRestoreConn);

    GWMMatch *swallows;
    int n = 0;
    TAILQ_FOREACH (swallows, &(state->con->swallowHead), matches) {
        char *serialized = NULL;

#define APPEND_REGEX(reName) \
    do { \
        if (swallows->reName != NULL) { \
            serialized = g_strdup_printf("%s%s" #reName "=\"%s\"", (serialized ? serialized : "["), (serialized ? " " : ""), swallows->reName->pattern); \
        } \
    } while (0)

        APPEND_REGEX(class);
        APPEND_REGEX(instance);
        APPEND_REGEX(windowRole);
        APPEND_REGEX(title);
        APPEND_REGEX(machine);

        if (serialized == NULL) {
            DEBUG("This swallows specification is not serializable?!");
            continue;
        }

        serialized = g_strdup_printf("%s]", serialized);
        DEBUG("con %p (placeholder 0x%08x) line %d: %s", state->con, state->window, n, serialized);

        draw_util_text(serialized, &(state->surface), foreground, background,
                       TEXT_PADDING,
                       (n * (1 + TEXT_PADDING)) + TEXT_PADDING,
                       state->rect.width - 2 * TEXT_PADDING);
        n++;
        free(serialized);
    }

//    i3String *line = i3string_from_utf8("⌚");
//    int text_width = predict_text_width(line);
//    int x = (state->rect.width / 2) - (text_width / 2);
//    int y = (state->rect.height / 2) - (12 / 2);
//    draw_util_text(line, &(state->surface), foreground, background, x, y, text_width);
//    i3string_free(line);

    xcb_aux_sync(gsRestoreConn);
}