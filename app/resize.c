//
// Created by dingjing on 23-12-10.
//

#include "resize.h"

#include "val.h"
#include "log.h"
#include "drag.h"


struct CallbackParams
{
    GWMOrientation  orientation;
    GWMContainer*   output;
    xcb_window_t    helpwin;
    uint32_t*       newPosition;
    bool*           thresholdExceeded;
};

DRAGGING_CB(resizeCallback)
{
    const struct CallbackParams* params = extra;
    GWMContainer* output = params->output;
    DEBUG("new x = %d, y = %d\n", new_x, new_y);

    if (!*params->thresholdExceeded) {
        xcb_map_window(gConn, params->helpwin);
        if (params->orientation == HORIZON) {
            xcb_warp_pointer(gConn, XCB_NONE, event->root, 0, 0, 0, 0, *params->newPosition + new_x - event->root_x, new_y);
        }
        else {
            xcb_warp_pointer(gConn, XCB_NONE, event->root, 0, 0, 0, 0, new_x, *params->newPosition + new_y - event->root_y);
        }
        *params->thresholdExceeded = true;
        return;
    }

    if (params->orientation == HORIZON) {
        if (new_x > (output->rect.x + output->rect.width - 25) || new_x < (output->rect.x + 25)) {
            return;
        }
        *(params->newPosition) = new_x;
        xcb_configure_window(gConn, params->helpwin, XCB_CONFIG_WINDOW_X, params->newPosition);
    }
    else {
        if (new_y > (output->rect.y + output->rect.height - 25) || new_y < (output->rect.y + 25)) {
            return;
        }

        *(params->newPosition) = new_y;
        xcb_configure_window(gConn, params->helpwin, XCB_CONFIG_WINDOW_Y, params->newPosition);
    }

    xcb_flush(gConn);
}


double resize_percent_for_1px(GWMContainer *con)
{
    return 0;
}

bool resize_neighboring_cons(GWMContainer *first, GWMContainer *second, int px, int ppt)
{
    return 0;
}

bool resize_find_tiling_participants(GWMContainer **current, GWMContainer **other, GWMDirection direction, bool bothSides)
{
    return 0;
}

void resize_graphical_handler(GWMContainer *first, GWMContainer *second, GWMOrientation orientation, const xcb_button_press_event_t *event, bool useThreshold)
{

}
