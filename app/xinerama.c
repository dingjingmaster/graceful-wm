//
// Created by dingjing on 23-11-24.
//

#include "xinerama.h"

#include <xcb/xinerama.h>

#include "val.h"
#include "log.h"
#include "utils.h"
#include "types.h"
#include "output.h"
#include "randr.h"

static int gsNumScreens = 0;


static GWMOutput* get_screen_at (unsigned int x, unsigned int y)
{
    GWMOutput *output;
    TAILQ_FOREACH (output, &gOutputs, outputs) {
        if (output->rect.x == x && output->rect.y == y) {
            return output;
        }
    }

    return NULL;
}

/*
 * Gets the Xinerama screens and converts them to virtual Outputs (only one screen for two
 * Xinerama screen which are configured in clone mode) in the given screenlist
 *
 */
static void query_screens(xcb_connection_t *conn)
{
    xcb_xinerama_query_screens_reply_t *reply;
    xcb_xinerama_screen_info_t *screen_info;

    reply = xcb_xinerama_query_screens_reply(conn, xcb_xinerama_query_screens_unchecked(conn), NULL);
    if (!reply) {
        ERROR(_("Couldn't get Xinerama screens"));
        return;
    }
    screen_info = xcb_xinerama_query_screens_screen_info(reply);
    int screens = xcb_xinerama_query_screens_screen_info_length(reply);

    for (int screen = 0; screen < screens; screen++) {
        GWMOutput* s = get_screen_at(screen_info[screen].x_org, screen_info[screen].y_org);
        if (s != NULL) {
            DEBUG("Re-used old Xinerama screen %p", s);
            s->rect.width = MIN(s->rect.width, screen_info[screen].width);
            s->rect.height = MIN(s->rect.height, screen_info[screen].height);
        } else {
            s = calloc(1, sizeof(GWMOutput));
            GWMOutputName* outputName = calloc(1, sizeof(GWMOutputName));
            outputName->name = g_strdup_printf ("xinerama-%d", gsNumScreens);
            SLIST_INIT(&s->namesHead);
            SLIST_INSERT_HEAD(&s->namesHead, outputName, names);
            DEBUG(_("Created new Xinerama screen %s (%p)"), output_primary_name(s), s);
            s->active = true;
            s->rect.x = screen_info[screen].x_org;
            s->rect.y = screen_info[screen].y_org;
            s->rect.width = screen_info[screen].width;
            s->rect.height = screen_info[screen].height;
            if (s->rect.x == 0 && s->rect.y == 0) {
                TAILQ_INSERT_HEAD(&gOutputs, s, outputs);
            }
            else {
                TAILQ_INSERT_TAIL(&gOutputs, s, outputs);
            }
            randr_output_init_container(s);
            randr_init_ws_for_output(s);
            gsNumScreens++;
        }

        DEBUG(_("found Xinerama screen: %d x %d at %d x %d"), screen_info[screen].width, screen_info[screen].height, screen_info[screen].x_org, screen_info[screen].y_org);
    }

    free(reply);

    if (gsNumScreens == 0) {
        ERROR(_("No screens found. Please fix your setup. i3 will exit now."));
        exit(EXIT_SUCCESS);
    }
}

/*
 * This creates the root_output (borrowed from randr.c) and uses it
 * as the sole output for this session.
 *
 */
static void use_root_output(xcb_connection_t *conn)
{
    GWMOutput* s = randr_create_root_output(conn);
    s->active = true;
    TAILQ_INSERT_TAIL(&gOutputs, s, outputs);
    randr_output_init_container(s);
    randr_init_ws_for_output(s);
}


void xinerama_init(void)
{
    if (!xcb_get_extension_data(gConn, &xcb_xinerama_id)->present) {
        DEBUG(_("Xinerama extension not found, using root output."));
        use_root_output(gConn);
    }
    else {
        xcb_xinerama_is_active_reply_t* reply = xcb_xinerama_is_active_reply(gConn, xcb_xinerama_is_active(gConn), NULL);
        if (reply == NULL || !reply->state) {
            DEBUG(_("Xinerama is not active (in your X-Server), using root output."));
            use_root_output(gConn);
        }
        else {
            query_screens(gConn);
        }

        FREE(reply);
    }
}

