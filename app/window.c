//
// Created by dingjing on 23-11-27.
//

#include "window.h"

#include "dpi.h"
#include "log.h"
#include "val.h"
#include "xcb.h"
#include "render.h"
#include "container.h"
#include "assignments.h"
#include "extend-wm-hints.h"


#define MWM_HINTS_FLAGS_FIELD 0
#define MWM_HINTS_DECORATIONS_FIELD 2

#define MWM_HINTS_DECORATIONS (1 << 1)
#define MWM_DECOR_ALL (1 << 0)
#define MWM_DECOR_BORDER (1 << 1)
#define MWM_DECOR_TITLE (1 << 3)


static GWMBorderStyle border_style_from_motif_value(uint32_t value);


void window_free(GWMWindow *win)
{
    FREE(win->classClass);
    FREE(win->classInstance);
    FREE(win->role);
    FREE(win->machine);
    FREE(win->name);
    cairo_surface_destroy(win->icon);
    FREE(win->ranAssignments);
    FREE(win);
}

void window_update_role(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("WM_WINDOW_ROLE not set.\n");
        FREE(prop);
        return;
    }

    char *new_role = g_strdup_printf("%.*s", xcb_get_property_value_length(prop), (char *)xcb_get_property_value(prop));
    FREE(win->role);
    win->role = new_role;
    DEBUG("WM_WINDOW_ROLE changed to \"%s\"\n", win->role);

    free(prop);
}

void window_update_icon(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    uint32_t *data = NULL;
    uint32_t width = 0, height = 0;
    uint64_t len = 0;
    const uint32_t pref_size = (uint32_t)(render_deco_height() - dpi_logical_px(2));

    if (!prop || prop->type != XCB_ATOM_CARDINAL || prop->format != 32) {
        DEBUG("_NET_WM_ICON is not set\n");
        FREE(prop);
        return;
    }

    uint32_t prop_value_len = xcb_get_property_value_length(prop);
    uint32_t *prop_value = (uint32_t *)xcb_get_property_value(prop);

    while (prop_value_len > (sizeof(uint32_t) * 2) && prop_value && prop_value[0] && prop_value[1]) {
        const uint32_t cur_width = prop_value[0];
        const uint32_t cur_height = prop_value[1];
        const uint64_t cur_len = cur_width * (uint64_t)cur_height;
        const uint64_t expected_len = (cur_len + 2) * 4;

        if (expected_len > prop_value_len) {
            break;
        }

        DEBUG("Found _NET_WM_ICON of size: (%d,%d)\n", cur_width, cur_height);

        const bool at_least_preferred_size = (cur_width >= pref_size && cur_height >= pref_size);
        const bool smaller_than_current = (cur_width < width || cur_height < height);
        const bool larger_than_current = (cur_width > width || cur_height > height);
        const bool not_yet_at_preferred = (width < pref_size || height < pref_size);
        if (len == 0
            || (at_least_preferred_size && (smaller_than_current || not_yet_at_preferred))
            || (!at_least_preferred_size && not_yet_at_preferred && larger_than_current)) {
            len = cur_len;
            width = cur_width;
            height = cur_height;
            data = prop_value;
        }

        if (width == pref_size && height == pref_size) {
            break;
        }

        prop_value_len -= expected_len;
        prop_value = (uint32_t *)(((uint8_t *)prop_value) + expected_len);
    }

    if (!data) {
        DEBUG("Could not get _NET_WM_ICON");
        FREE(prop);
        return;
    }

    DEBUG("Using icon of size (%d,%d) (preferred size: %d)", width, height, pref_size);

    win->nameXChanged = true; /* trigger a redraw */

    uint32_t *icon = g_malloc0(len * 4);

    for (uint64_t i = 0; i < len; i++) {
        uint8_t r, g, b, a;
        const uint32_t pixel = data[2 + i];
        a = (pixel >> 24) & 0xff;
        r = (pixel >> 16) & 0xff;
        g = (pixel >> 8) & 0xff;
        b = (pixel >> 0) & 0xff;

        r = (r * a) / 0xff;
        g = (g * a) / 0xff;
        b = (b * a) / 0xff;

        icon[i] = ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
    }

    if (win->icon != NULL) {
        cairo_surface_destroy(win->icon);
    }
    win->icon = cairo_image_surface_create_for_data((unsigned char *)icon, CAIRO_FORMAT_ARGB32, width, height, width * 4);
    static cairo_user_data_key_t free_data;
    cairo_surface_set_user_data(win->icon, &free_data, icon, free);

    FREE(prop);
}

void window_update_name(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("_NET_WM_NAME not specified, not changing\n");
        FREE(prop);
        return;
    }

    FREE(win->name);

    /* Truncate the name at the first zero byte. See #3515. */
    const int len = xcb_get_property_value_length(prop);
    char *name = g_strndup(xcb_get_property_value(prop), len);
    win->name = (name);
    free(name);

    GWMContainer* con = container_by_window_id(win->id);
    if (con != NULL && con->titleFormat != NULL) {
        g_autofree char* name = container_parse_title_format(con);
        extend_wm_hint_update_visible_name(win->id, (name));
    }
    win->nameXChanged = true;
    DEBUG ("_NET_WM_NAME changed to \"%s\"\n", (win->name));

    win->usesNetWMName = true;

    free(prop);
}

void window_update_class(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("WM_CLASS not set.\n");
        FREE(prop);
        return;
    }

    const size_t prop_length = xcb_get_property_value_length(prop);
    char *new_class = xcb_get_property_value(prop);
    const size_t class_class_index = strnlen(new_class, prop_length) + 1;

    FREE(win->classInstance);
    FREE(win->classClass);

    win->classInstance = g_strndup(new_class, prop_length);
    if (class_class_index < prop_length) {
        win->classClass = g_strndup(new_class + class_class_index, prop_length - class_class_index);
    }
    else {
        win->classClass = NULL;
    }
    DEBUG("WM_CLASS changed to %s (instance), %s (class)", win->classInstance, win->classClass);

    free(prop);
}

void window_update_leader(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("CLIENT_LEADER not set on window 0x%08x.\n", win->id);
        win->leader = XCB_NONE;
        FREE(prop);
        return;
    }

    xcb_window_t *leader = xcb_get_property_value(prop);
    if (leader == NULL) {
        free(prop);
        return;
    }

    DEBUG("Client leader changed to %08x\n", *leader);

    win->leader = *leader;

    free(prop);
}

void window_update_machine(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("WM_CLIENT_MACHINE not set.\n");
        FREE(prop);
        return;
    }

    FREE(win->machine);
    win->machine = g_strndup((char *)xcb_get_property_value(prop), xcb_get_property_value_length(prop));
    DEBUG("WM_CLIENT_MACHINE changed to \"%s\"\n", win->machine);

    free(prop);
}

void window_update_type(GWMWindow *window, xcb_get_property_reply_t *reply)
{
    xcb_atom_t new_type = xcb_gwm_get_preferred_window_type(reply);
    free(reply);
    if (new_type == XCB_NONE) {
        DEBUG("cannot read _NET_WM_WINDOW_TYPE from window.\n");
        return;
    }

    window->windowType = new_type;
    DEBUG("_NET_WM_WINDOW_TYPE changed to %i.\n", window->windowType);

    assignments_run(window);
}

void window_update_name_legacy(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("WM_NAME not set (_NET_WM_NAME is what you want anyways).\n");
        FREE(prop);
        return;
    }

    /* ignore update when the window is known to already have a UTF-8 name */
    if (win->usesNetWMName) {
        free(prop);
        return;
    }

    FREE(win->name);
    const int len = xcb_get_property_value_length(prop);
    char *name = g_strndup(xcb_get_property_value(prop), len);
    win->name = (name);
    free(name);

    GWMContainer *con = container_by_window_id(win->id);
    if (con != NULL && con->titleFormat != NULL) {
        g_autofree char*name = container_parse_title_format(con);
        extend_wm_hint_update_visible_name(win->id, (name));
    }

    DEBUG("WM_NAME changed to \"%s\"\n", (win->name));
    DEBUG("Using legacy window title. Note that in order to get Unicode window titles in i3, the application has to set _NET_WM_NAME (UTF-8)\n");

    win->nameXChanged = true;

    free(prop);
}

void window_update_strut_partial(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("_NET_WM_STRUT_PARTIAL not set.\n");
        FREE(prop);
        return;
    }

    uint32_t *strut;
    if (!(strut = xcb_get_property_value(prop))) {
        free(prop);
        return;
    }

    DEBUG("Reserved pixels changed to: left = %d, right = %d, top = %d, bottom = %d", strut[0], strut[1], strut[2], strut[3]);

    win->reserved = (GWMReserveEdgePixels){strut[0], strut[1], strut[2], strut[3]};

    free(prop);
}

void window_update_transient_for(GWMWindow *win, xcb_get_property_reply_t *prop)
{
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("TRANSIENT_FOR not set on window 0x%08x.\n", win->id);
        win->transientFor = XCB_NONE;
        FREE(prop);
        return;
    }

    xcb_window_t transient_for;
    if (!xcb_icccm_get_wm_transient_for_from_reply(&transient_for, prop)) {
        free(prop);
        return;
    }

    DEBUG("Transient for changed to 0x%08x (window 0x%08x)\n", transient_for, win->id);

    win->transientFor = transient_for;

    free(prop);
}

void window_update_hints(GWMWindow *win, xcb_get_property_reply_t *prop, bool *urgencyHint)
{
    if (urgencyHint != NULL) {
        *urgencyHint = false;
    }

    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DEBUG("WM_HINTS not set.\n");
        FREE(prop);
        return;
    }

    xcb_icccm_wm_hints_t hints;

    if (!xcb_icccm_get_wm_hints_from_reply(&hints, prop)) {
        DEBUG("Could not get WM_HINTS\n");
        free(prop);
        return;
    }

    if (hints.flags & XCB_ICCCM_WM_HINT_INPUT) {
        win->doesNotAcceptFocus = !hints.input;
        DEBUG("WM_HINTS.input changed to \"%d\"\n", hints.input);
    }

    if (urgencyHint != NULL) {
        *urgencyHint = (xcb_icccm_wm_hints_get_urgency(&hints) != 0);
    }

    free(prop);
}

bool window_update_motif_hints(GWMWindow *win, xcb_get_property_reply_t *prop, GWMBorderStyle *motifBorderStyle)
{
    if (prop == NULL) {
        return false;
    }
    g_assert(motifBorderStyle != NULL);

    if (xcb_get_property_value_length(prop) == 0) {
        FREE(prop);
        return false;
    }

    uint32_t *motif_hints = (uint32_t *)xcb_get_property_value(prop);

    if (motif_hints[MWM_HINTS_FLAGS_FIELD] & MWM_HINTS_DECORATIONS) {
        *motifBorderStyle = border_style_from_motif_value(motif_hints[MWM_HINTS_DECORATIONS_FIELD]);
        FREE(prop);
        return true;
    }
    FREE(prop);
    return false;
}

bool window_update_normal_hints(GWMWindow *win, xcb_get_property_reply_t *reply, xcb_get_geometry_reply_t *geom)
{
    bool changed = false;
    xcb_size_hints_t size_hints;

    /* If the hints were already in this event, use them, if not, request them */
    bool success;
    if (reply != NULL) {
        success = xcb_icccm_get_wm_size_hints_from_reply(&size_hints, reply);
    } else {
        success = xcb_icccm_get_wm_normal_hints_reply(gConn, xcb_icccm_get_wm_normal_hints_unchecked(gConn, win->id), &size_hints, NULL);
    }
    if (!success) {
        DEBUG("Could not get WM_NORMAL_HINTS\n");
        return false;
    }

#define ASSIGN_IF_CHANGED(original, new) \
    do {                                 \
        if (original != new) {           \
            original = new;              \
            changed = true;              \
        }                                \
    } while (0)

    if ((size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
        DEBUG("Minimum size: %d (width) x %d (height)\n", size_hints.min_width, size_hints.min_height);

        ASSIGN_IF_CHANGED(win->minWidth, size_hints.min_width);
        ASSIGN_IF_CHANGED(win->minHeight, size_hints.min_height);
    }

    if ((size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
        DEBUG("Maximum size: %d (width) x %d (height)\n", size_hints.max_width, size_hints.max_height);

        int max_width = MAX(0, size_hints.max_width);
        int max_height = MAX(0, size_hints.max_height);

        ASSIGN_IF_CHANGED(win->maxWidth, max_width);
        ASSIGN_IF_CHANGED(win->maxHeight, max_height);
    }
    else {
        DEBUG("Clearing maximum size\n");

        ASSIGN_IF_CHANGED(win->maxWidth, 0);
        ASSIGN_IF_CHANGED(win->maxHeight, 0);
    }

    if ((size_hints.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC)) {
        DEBUG("Size increments: %d (width) x %d (height)\n", size_hints.width_inc, size_hints.height_inc);

        if (size_hints.width_inc > 0 && size_hints.width_inc < 0xFFFF) {
            ASSIGN_IF_CHANGED(win->widthInc, size_hints.width_inc);
        }
        else {
            ASSIGN_IF_CHANGED(win->widthInc, 0);
        }

        if (size_hints.height_inc > 0 && size_hints.height_inc < 0xFFFF) {
            ASSIGN_IF_CHANGED(win->heightInc, size_hints.height_inc);
        } else {
            ASSIGN_IF_CHANGED(win->heightInc, 0);
        }
    }
    else {
        DEBUG("Clearing size increments\n");

        ASSIGN_IF_CHANGED(win->widthInc, 0);
        ASSIGN_IF_CHANGED(win->heightInc, 0);
    }

    if (size_hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE && (win->baseWidth >= 0) && (win->baseHeight >= 0)) {
        DEBUG("Base size: %d (width) x %d (height)\n", size_hints.base_width, size_hints.base_height);

        ASSIGN_IF_CHANGED(win->baseWidth, size_hints.base_width);
        ASSIGN_IF_CHANGED(win->baseHeight, size_hints.base_height);
    }
    else {
        DEBUG("Clearing base size\n");

        ASSIGN_IF_CHANGED(win->baseWidth, 0);
        ASSIGN_IF_CHANGED(win->baseHeight, 0);
    }

    if (geom != NULL &&
        (size_hints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION || size_hints.flags & XCB_ICCCM_SIZE_HINT_P_POSITION) &&
        (size_hints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE || size_hints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
        DEBUG("Setting geometry x=%d y=%d w=%d h=%d\n", size_hints.x, size_hints.y, size_hints.width, size_hints.height);
        geom->x = size_hints.x;
        geom->y = size_hints.y;
        geom->width = size_hints.width;
        geom->height = size_hints.height;
    }

    if (size_hints.flags & XCB_ICCCM_SIZE_HINT_P_ASPECT &&
        (size_hints.min_aspect_num >= 0) && (size_hints.min_aspect_den > 0) &&
        (size_hints.max_aspect_num >= 0) && (size_hints.max_aspect_den > 0)) {
        double min_aspect = (double)size_hints.min_aspect_num / size_hints.min_aspect_den;
        double max_aspect = (double)size_hints.max_aspect_num / size_hints.max_aspect_den;
        DEBUG("Aspect ratio set: minimum %f, maximum %f\n", min_aspect, max_aspect);
        if (ABS(win->minAspectRatio - min_aspect) > DBL_EPSILON) {
            win->minAspectRatio = min_aspect;
            changed = true;
        }
        if (ABS(win->maxAspectRatio - max_aspect) > DBL_EPSILON) {
            win->maxAspectRatio = max_aspect;
            changed = true;
        }
    }
    else {
        DEBUG("Clearing aspect ratios\n");

        ASSIGN_IF_CHANGED(win->minAspectRatio, 0.0);
        ASSIGN_IF_CHANGED(win->maxAspectRatio, 0.0);
    }

    return changed;
}


static GWMBorderStyle border_style_from_motif_value(uint32_t value)
{
    if (value & MWM_DECOR_ALL) {
        if (value & MWM_DECOR_TITLE) {
            if (value & MWM_DECOR_BORDER) {
                return BS_NONE;
            }
            return BS_PIXEL;
        }

        return BS_NORMAL;
    }
    else if (value & MWM_DECOR_TITLE) {
        return BS_NORMAL;
    }
    else if (value & MWM_DECOR_BORDER) {
        return BS_PIXEL;
    }
    else {
        return BS_NONE;
    }
}

#undef MWM_HINTS_FLAGS_FIELD
#undef MWM_HINTS_DECORATIONS_FIELD
#undef MWM_HINTS_DECORATIONS
#undef MWM_DECOR_ALL
#undef MWM_DECOR_BORDER
#undef MWM_DECOR_TITLE