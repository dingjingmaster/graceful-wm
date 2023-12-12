//
// Created by dingjing on 23-12-8.
//

#include "match.h"

#include "log.h"
#include "val.h"
#include "container.h"
#include "utils.h"


#define _gwm_timercmp(a, b, CMP) \
    (((a).tv_sec == (b).tv_sec) ? ((a).tv_usec CMP(b).tv_usec) : ((a).tv_sec CMP(b).tv_sec))


void match_init(GWMMatch *match)
{
    memset(match, 0, sizeof(GWMMatch));
    match->urgent = U_DO_NOT_CHECK;
    match->windowMode = WM_ANY;
    match->windowType = UINT32_MAX;
}

bool match_is_empty(GWMMatch *match)
{
    return (match->title == NULL && match->mark == NULL && match->application == NULL && match->class == NULL && match->instance == NULL && match->windowRole == NULL && match->workspace == NULL && match->machine == NULL && match->urgent == U_DO_NOT_CHECK && match->id == XCB_NONE && match->windowType == UINT32_MAX && match->containerID == NULL && match->dock == M_NO_DOCK && match->windowMode == WM_ANY && match->matchAllWindows == false);
}

void match_copy(GWMMatch *dest, GWMMatch *src)
{
    memcpy(dest, src, sizeof(GWMMatch));

#define DUPLICATE_REGEX(field)                            \
    do {                                                  \
        if (src->field != NULL)                           \
            dest->field = regex_new(src->field->pattern); \
    } while (0)

    DUPLICATE_REGEX(title);
    DUPLICATE_REGEX(mark);
    DUPLICATE_REGEX(application);
    DUPLICATE_REGEX(class);
    DUPLICATE_REGEX(instance);
    DUPLICATE_REGEX(windowRole);
    DUPLICATE_REGEX(workspace);
    DUPLICATE_REGEX(machine);
}

bool match_matches_window(GWMMatch *match, GWMWindow *window)
{
    INFO("Checking window 0x%08x (class %s)", window->id, window->classClass);

#define GET_FIELD_str(field) (field)
#define GET_FIELD_gwm_string(field) ((field))
#define CHECK_WINDOW_FIELD(match_field, window_field, type)                                       \
    do {                                                                                          \
        if (match->match_field != NULL) {                                                         \
            const char *window_field_str = window->window_field == NULL                           \
                                               ? ""                                               \
                                               : GET_FIELD_##type(window->window_field);          \
            if (strcmp(match->match_field->pattern, "__focused__") == 0 &&                        \
                gFocused && gFocused->window && gFocused->window->window_field &&                 \
                strcmp(window_field_str, GET_FIELD_##type(gFocused->window->window_field)) == 0) {\
                INFO("window " #match_field " matches focused window");                         \
            }                                                                                     \
            else if (regex_matches(match->match_field, window_field_str)) {                       \
                INFO("window " #match_field " matches (%s)", window_field_str);                 \
            }                                                                                     \
            else {                                                                                \
                return false;                                                                     \
            }                                                                                     \
        }                                                                                         \
    } while (0)

    CHECK_WINDOW_FIELD(class, classClass, str);
    CHECK_WINDOW_FIELD(instance, classInstance, str);

    if (match->id != XCB_NONE) {
        if (window->id == match->id) {
            INFO("match made by window id (%d)", window->id);
        } else {
            INFO("window id does not match");
            return false;
        }
    }

    CHECK_WINDOW_FIELD(title, name, gwm_string);
    CHECK_WINDOW_FIELD(windowRole, role, gwm_string);

    if (match->windowType != UINT32_MAX) {
        if (window->windowType == match->windowType) {
            INFO("window_type matches (%i)", match->windowType);
        }
        else {
            return false;
        }
    }

    CHECK_WINDOW_FIELD(machine, machine, str);

    GWMContainer* con = NULL;
    if (match->urgent == U_LATEST) {
        /* if the window isn't urgent, no sense in searching */
        if (window->urgent.tv_sec == 0) {
            return false;
        }
        /* if we find a window that is newer than this one, bail */
        TAILQ_FOREACH (con, &gAllContainer, allContainers) {
            if ((con->window != NULL) &&
                _gwm_timercmp(con->window->urgent, window->urgent, >)) {
                return false;
            }
        }
        INFO("urgent matches latest");
    }

    if (match->urgent == U_OLDEST) {
        /* if the window isn't urgent, no sense in searching */
        if (window->urgent.tv_sec == 0) {
            return false;
        }
        /* if we find a window that is older than this one (and not 0), bail */
        TAILQ_FOREACH (con, &gAllContainer, allContainers) {
            if ((con->window != NULL) &&
                (con->window->urgent.tv_sec != 0) &&
                _gwm_timercmp(con->window->urgent, window->urgent, <)) {
                return false;
            }
        }
        INFO("urgent matches oldest");
    }

    if (match->workspace != NULL) {
        if ((con = container_by_window_id(window->id)) == NULL)
            return false;

        GWMContainer* ws = container_get_workspace(con);
        if (ws == NULL)
            return false;

        if (strcmp(match->workspace->pattern, "__focused__") == 0 &&
            strcmp(ws->name, container_get_workspace(gFocused)->name) == 0) {
            INFO("workspace matches focused workspace");
        }
        else if (regex_matches(match->workspace, ws->name)) {
            INFO("workspace matches (%s)", ws->name);
        }
        else {
            return false;
        }
    }

    if (match->dock != M_DO_NOT_CHECK) {
        if ((window->dock == W_DOCK_TOP && match->dock == M_DOCK_TOP) ||
            (window->dock == W_DOCK_BOTTOM && match->dock == M_DOCK_BOTTOM) ||
            ((window->dock == W_DOCK_TOP || window->dock == W_DOCK_BOTTOM) &&
             match->dock == M_DOCK_ANY) ||
            (window->dock == W_NO_DOCK && match->dock == M_NO_DOCK)) {
            INFO("dock status matches");
        }
        else {
            INFO("dock status does not match");
            return false;
        }
    }

    if (match->mark != NULL) {
        if ((con = container_by_window_id(window->id)) == NULL)
            return false;

        bool matched = false;
        GWMMark* mark;
        TAILQ_FOREACH (mark, &(con->marksHead), marks) {
            if (regex_matches(match->mark, mark->name)) {
                matched = true;
                break;
            }
        }

        if (matched) {
            INFO("mark matches");
        }
        else {
            INFO("mark does not match");
            return false;
        }
    }

    if (match->windowMode != WM_ANY) {
        if ((con = container_by_window_id(window->id)) == NULL) {
            return false;
        }

        switch (match->windowMode) {
            case WM_TILING_AUTO:
                if (con->floating != FLOATING_AUTO_OFF) {
                    return false;
                }
                break;
            case WM_TILING_USER:
                if (con->floating != FLOATING_USER_OFF) {
                    return false;
                }
                break;
            case WM_TILING:
                if (container_inside_floating(con) != NULL) {
                    return false;
                }
                break;
            case WM_FLOATING_AUTO:
                if (con->floating != FLOATING_AUTO_ON) {
                    return false;
                }
                break;
            case WM_FLOATING_USER:
                if (con->floating != FLOATING_USER_ON) {
                    return false;
                }
                break;
            case WM_FLOATING:
                if (container_inside_floating(con) == NULL) {
                    return false;
                }
                break;
            case WM_ANY:
                g_assert(false);
        }

        INFO("window_mode matches");
    }

    /* NOTE: See the comment regarding 'all' in match_parse_property()
     * for an explanation of why match_all_windows isn't explicitly
     * checked. */

    return true;
}

void match_free(GWMMatch *match)
{
    FREE(match->error);
    regex_free(match->title);
    regex_free(match->application);
    regex_free(match->class);
    regex_free(match->instance);
    regex_free(match->mark);
    regex_free(match->windowRole);
    regex_free(match->workspace);
    regex_free(match->machine);
}

void match_parse_property(GWMMatch *match, const char *cType, const char *cValue)
{
    g_assert(match != NULL);
    DEBUG("cType=*%s*, cValue=*%s*", cType, cValue);

    if (strcmp(cType, "class") == 0) {
        regex_free(match->class);
        match->class = regex_new(cValue);
        return;
    }

    if (strcmp(cType, "instance") == 0) {
        regex_free(match->instance);
        match->instance = regex_new(cValue);
        return;
    }

    if (strcmp(cType, "window_role") == 0) {
        regex_free(match->windowRole);
        match->windowRole = regex_new(cValue);
        return;
    }

    if (strcmp(cType, "con_id") == 0) {
        if (strcmp(cValue, "__focused__") == 0) {
            match->containerID = gFocused;
            return;
        }

        long parsed;
        if (!util_parse_long(cValue, &parsed, 0)) {
            ERROR("Could not parse con id \"%s\"", cValue);
            match->error = g_strdup("invalid con_id");
        } else {
            match->containerID = (GWMContainer*)parsed;
            DEBUG("id as int = %p", match->containerID);
        }
        return;
    }

    if (strcmp(cType, "id") == 0) {
        long parsed;
        if (!util_parse_long(cValue, &parsed, 0)) {
            ERROR("Could not parse window id \"%s\"", cValue);
            match->error = g_strdup("invalid id");
        }
        else {
            match->id = parsed;
            DEBUG("window id as int = %d", match->id);
        }
        return;
    }

    if (strcmp(cType, "window_type") == 0) {
        if (strcasecmp(cValue, "normal") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_NORMAL;
        } else if (strcasecmp(cValue, "dialog") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_DIALOG;
        } else if (strcasecmp(cValue, "utility") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_UTILITY;
        } else if (strcasecmp(cValue, "toolbar") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_TOOLBAR;
        } else if (strcasecmp(cValue, "splash") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_SPLASH;
        } else if (strcasecmp(cValue, "menu") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_MENU;
        } else if (strcasecmp(cValue, "dropdown_menu") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
        } else if (strcasecmp(cValue, "popup_menu") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_POPUP_MENU;
        } else if (strcasecmp(cValue, "tooltip") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_TOOLTIP;
        } else if (strcasecmp(cValue, "notification") == 0) {
            match->windowType = A__NET_WM_WINDOW_TYPE_NOTIFICATION;
        }
        else {
            ERROR("unknown window_type value \"%s\"", cValue);
            match->error = g_strdup("unknown window_type value");
        }

        return;
    }

    if (strcmp(cType, "con_mark") == 0) {
        regex_free(match->mark);
        match->mark = regex_new(cValue);
        return;
    }

    if (strcmp(cType, "title") == 0) {
        regex_free(match->title);
        match->title = regex_new(cValue);
        return;
    }

    if (strcmp(cType, "urgent") == 0) {
        if (strcasecmp(cValue, "latest") == 0 ||
            strcasecmp(cValue, "newest") == 0 ||
            strcasecmp(cValue, "recent") == 0 ||
            strcasecmp(cValue, "last") == 0) {
            match->urgent = U_LATEST;
        } else if (strcasecmp(cValue, "oldest") == 0 ||
                   strcasecmp(cValue, "first") == 0) {
            match->urgent = U_OLDEST;
        }
        return;
    }

    if (strcmp(cType, "workspace") == 0) {
        regex_free(match->workspace);
        match->workspace = regex_new(cValue);
        return;
    }

    if (strcmp(cType, "machine") == 0) {
        regex_free(match->machine);
        match->machine = regex_new(cValue);
        return;
    }

    if (strcmp(cType, "tiling") == 0) {
        match->windowMode = WM_TILING;
        return;
    }

    if (strcmp(cType, "tiling_from") == 0 &&
        cValue != NULL &&
        strcmp(cValue, "auto") == 0) {
        match->windowMode = WM_TILING_AUTO;
        return;
    }

    if (strcmp(cType, "tiling_from") == 0 &&
        cValue != NULL &&
        strcmp(cValue, "user") == 0) {
        match->windowMode = WM_TILING_USER;
        return;
    }

    if (strcmp(cType, "floating") == 0) {
        match->windowMode = WM_FLOATING;
        return;
    }

    if (strcmp(cType, "floating_from") == 0 &&
        cValue != NULL &&
        strcmp(cValue, "auto") == 0) {
        match->windowMode = WM_FLOATING_AUTO;
        return;
    }

    if (strcmp(cType, "floating_from") == 0 &&
        cValue != NULL &&
        strcmp(cValue, "user") == 0) {
        match->windowMode = WM_FLOATING_USER;
        return;
    }

    /* match_matches_window() only checks negatively, so match_all_windows
     * won't actually be used there, but that's OK because if no negative
     * match is found (e.g. because of a more restrictive criterion) the
     * return value of match_matches_window() is true.
     * Setting it here only serves to cause match_is_empty() to return false,
     * otherwise empty criteria rules apply, and that's not what we want. */
    if (strcmp(cType, "all") == 0) {
        match->matchAllWindows = true;
        return;
    }

    ERROR("Unknown criterion: %s", cType);
}
