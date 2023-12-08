//
// Created by dingjing on 23-11-24.
//

#include "key-bindings.h"

#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "log.h"
#include "val.h"
#include "xcb.h"
#include "types.h"
#include "utils.h"
#include "config.h"

#define ADD_TRANSLATED_KEY(code, mods)                                              \
    do {                                                                            \
        GWMBindingKeycode* bindingKeycode = g_malloc0(sizeof(GWMBindingKeycode));   \
        bindingKeycode->modifiers = (mods);                                         \
        bindingKeycode->keycode = (code);                                           \
        GList* node = g_malloc0(sizeof(GList));                                     \
        node->data = bindingKeycode;                                                \
        g_queue_push_tail_link(&(bind->keycodesHead), node);                        \
    } while (0)


struct resolve
{
    /* The binding which we are resolving. */
    GWMBinding*             bind;

    /* |bind|’s keysym (translated to xkb_keysym_t), e.g. XKB_KEY_R. */
    xkb_keysym_t            keysym;

    /* The xkb state built from the user-provided modifiers and group. */
    struct xkb_state*       xkbState;

    /* Like |xkb_state|, just without the shift modifier, if shift was specified. */
    struct xkb_state*       xkbStateNoShift;

    /* Like |xkb_state|, but with NumLock. */
    struct xkb_state*       xkbStateNumlock;

    /* Like |xkb_state|, but with NumLock, just without the shift modifier, if shift was specified. */
    struct xkb_state*       xkbStateNumlockNoShift;
};


static void reorder_bindings_of_mode(GWMConfigMode *mode);
static bool binding_same_key(GWMBinding* a, GWMBinding* b);
static int reorder_binding_cmp(const void *a, const void *b);
static bool binding_in_current_group(const GWMBinding *bind);
static int fill_rmlvo_from_root(struct xkb_rule_names *xkbNames);
static GWMConfigMode* mode_from_name(const char *name, bool pangoMarkup);
static void add_keycode_if_matches(struct xkb_keymap *keymap, xkb_keycode_t key, void *data);
static void grab_keycode_for_binding(xcb_connection_t* conn, GWMBinding* bind, uint32_t keycode);


static struct xkb_keymap*       gXKBKeymap;
static struct xkb_context*      gXKBContext;
pid_t                           gCommandErrorNagBarPid = -1;
const char*                     DEFAULT_BINDING_MODE = "default";


bool key_binding_load_keymap(void)
{
    if (NULL == gXKBContext) {
        if ((gXKBContext = xkb_context_new(0)) == NULL) {
            ERROR(_("Could not create xkbcommon context"));
            return false;
        }
    }

    int32_t deviceId;
    struct xkb_keymap *newKeymap = NULL;
    if (gXKBSupported && (deviceId = xkb_x11_get_core_keyboard_device_id(gConn)) > -1) {
        if ((newKeymap = xkb_x11_keymap_new_from_device(gXKBContext, gConn, deviceId, 0)) == NULL) {
            ERROR(_("xkb_x11_keymap_new_from_device failed"));
            return false;
        }
    }
    else {
        DEBUG(_("No XKB / core keyboard device? Assembling keymap from local RMLVO."));
        struct xkb_rule_names names = {
            .rules = NULL,
            .model = NULL,
            .layout = NULL,
            .variant = NULL,
            .options = NULL};
        if (fill_rmlvo_from_root(&names) == -1) {
            ERROR(_("Could not get _XKB_RULES_NAMES atom from root window, falling back to defaults."));
        }
        newKeymap = xkb_keymap_new_from_names(gXKBContext, &names, 0);
        free((char *)names.rules);
        free((char *)names.model);
        free((char *)names.layout);
        free((char *)names.variant);
        free((char *)names.options);
        if (newKeymap == NULL) {
            ERROR(_("xkb_keymap_new_from_names failed"));
            return false;
        }
    }
    xkb_keymap_unref(gXKBKeymap);
    gXKBKeymap = newKeymap;

    return true;
}

void key_binding_translate_keysyms(void)
{
    bool hasErrors = false;
    struct xkb_state* dummyState = NULL;
    struct xkb_state* dummyStateNoShift = NULL;
    struct xkb_state* dummyStateNumlock = NULL;
    struct xkb_state* dummyStateNumlockNoShift = NULL;

    if (NULL == (dummyState = xkb_state_new(gXKBKeymap))
        || NULL == (dummyStateNoShift = xkb_state_new(gXKBKeymap))
        || NULL == (dummyStateNumlock = xkb_state_new(gXKBKeymap))
        || NULL == (dummyStateNumlockNoShift = xkb_state_new(gXKBKeymap))) {
        ERROR(_("Could not create XKB state, cannot translate keysyms."));
        goto out;
    }

    GWMBinding* bind = NULL;
    for (GList* ls = gBindings; ls; ls = ls->next) {
        bind = ls->data;
        if (B_MOUSE == bind->inputType) {
            long button;
            if (!util_parse_long(bind->symbol + (sizeof("button") - 1), &button, 10)) {
                ERROR(_("Could not translate string to button: \"%s\""), bind->symbol);
            }

            xcb_keycode_t key = button;
            bind->keycode = key;
            DEBUG(_("Binding Mouse button, Keycode = %d"), key);
        }

        xkb_layout_index_t group = XCB_XKB_GROUP_1;
        if ((bind->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_2) {
            group = XCB_XKB_GROUP_2;
        }
        else if ((bind->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_3) {
            group = XCB_XKB_GROUP_3;
        }
        else if ((bind->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_4) {
            group = XCB_XKB_GROUP_4;
        }

        DEBUG(_("Binding %p group = %d, event_state_mask = %d, &2 = %s, &3 = %s, &4 = %s"), \
                bind, group, bind->eventStateMask, \
                (bind->eventStateMask & GWM_XKB_GROUP_MASK_2) ? "yes" : "no", \
                (bind->eventStateMask & GWM_XKB_GROUP_MASK_3) ? "yes" : "no", \
                (bind->eventStateMask & GWM_XKB_GROUP_MASK_4) ? "yes" : "no");
        (void) xkb_state_update_mask(
            dummyState,
            (bind->eventStateMask & 0x1FFF) /* xkb_mod_mask_t base_mods, */,
            0 /* xkb_mod_mask_t latched_mods, */,
            0 /* xkb_mod_mask_t locked_mods, */,
            0 /* xkb_layout_index_t base_group, */,
            0 /* xkb_layout_index_t latched_group, */,
            group /* xkb_layout_index_t locked_group, */);

        (void) xkb_state_update_mask(
            dummyStateNoShift,
            (bind->eventStateMask & 0x1FFF) ^ XCB_KEY_BUT_MASK_SHIFT /* xkb_mod_mask_t base_mods, */,
            0 /* xkb_mod_mask_t latched_mods, */,
            0 /* xkb_mod_mask_t locked_mods, */,
            0 /* xkb_layout_index_t base_group, */,
            0 /* xkb_layout_index_t latched_group, */,
            group /* xkb_layout_index_t locked_group, */);

        (void) xkb_state_update_mask(
            dummyStateNumlock,
            (bind->eventStateMask & 0x1FFF) | gXCBNumLockMask /* xkb_mod_mask_t base_mods, */,
            0 /* xkb_mod_mask_t latched_mods, */,
            0 /* xkb_mod_mask_t locked_mods, */,
            0 /* xkb_layout_index_t base_group, */,
            0 /* xkb_layout_index_t latched_group, */,
            group /* xkb_layout_index_t locked_group, */);

        (void) xkb_state_update_mask(
            dummyStateNumlockNoShift,
            ((bind->eventStateMask & 0x1FFF) | gXCBNumLockMask) ^ XCB_KEY_BUT_MASK_SHIFT /* xkb_mod_mask_t base_mods, */,
            0 /* xkb_mod_mask_t latched_mods, */,
            0 /* xkb_mod_mask_t locked_mods, */,
            0 /* xkb_layout_index_t base_group, */,
            0 /* xkb_layout_index_t latched_group, */,
            group /* xkb_layout_index_t locked_group, */);

        if (bind->keycode > 0) {
            /* We need to specify modifiers for the keycode binding (numlock
             * fallback). */
            g_queue_clear_full (bind->keycodesHead.head, g_free);
            g_queue_init (&(bind->keycodesHead));

            ADD_TRANSLATED_KEY(bind->keycode, bind->eventStateMask);

            /* Also bind the key with active CapsLock */
            ADD_TRANSLATED_KEY(bind->keycode, bind->eventStateMask | XCB_MOD_MASK_LOCK);

            /* If this binding is not explicitly for NumLock, check whether we need to
             * add a fallback. */
            if ((bind->eventStateMask & gXCBNumLockMask) != gXCBNumLockMask) {
                xkb_keysym_t sym = xkb_state_key_get_one_sym(dummyState, bind->keycode);
                xkb_keysym_t symNumlock = xkb_state_key_get_one_sym(dummyStateNumlock, bind->keycode);
                if (sym == symNumlock) {
                    ADD_TRANSLATED_KEY(bind->keycode, bind->eventStateMask | gXCBNumLockMask);
                    ADD_TRANSLATED_KEY(bind->keycode, bind->eventStateMask | gXCBNumLockMask | XCB_MOD_MASK_LOCK);
                }
                else {
                    DEBUG(_("Skipping automatic numlock fallback, key %d resolves to 0x%x with numlock"), bind->keycode, symNumlock);
                }
            }
            continue;
        }

        const xkb_keysym_t keysym = xkb_keysym_from_name(bind->symbol, XKB_KEYSYM_NO_FLAGS);
        if (keysym == XKB_KEY_NoSymbol) {
            ERROR(_("Could not translate string to key symbol: \"%s\""), bind->symbol);
            continue;
        }

        struct resolve resolving = {
            .bind = bind,
            .keysym = keysym,
            .xkbState = dummyState,
            .xkbStateNoShift = dummyStateNoShift,
            .xkbStateNumlock = dummyStateNumlock,
            .xkbStateNumlockNoShift = dummyStateNumlockNoShift,
        };
        g_queue_clear_full (&(bind->keycodesHead), g_free);
        g_queue_init (&(bind->keycodesHead));

        xkb_keymap_key_for_each(gXKBKeymap, add_keycode_if_matches, &resolving);
        char* keycodes = g_strdup("");
        int numKeycodes = 0;
        GWMBindingKeycode* bindingKeycode = NULL;
        for (GList* ls = bind->keycodesHead.head; ls; ls = ls->next) {
            bindingKeycode = ls->data;
            char* tmp = g_strdup_printf ("%s %d", keycodes, bindingKeycode->keycode);
            free(keycodes);
            keycodes = tmp;
            numKeycodes++;

            /* check for duplicate bindings */
            GWMBinding* check = NULL;
            for (GList* ls1 = gBindings; ls1; ls1 = ls1->next) {
                if (check == bind) {
                    continue;
                }

                if (check->symbol != NULL) {
                    continue;
                }

                if (check->keycode != bindingKeycode->keycode
                    || check->eventStateMask != bindingKeycode->modifiers
                    || check->release != bind->release) {
                    continue;
                }
                hasErrors = true;
                ERROR(_("Duplicate keybinding in config file:\n  keysym = %s, keycode = %d, state_mask = 0x%x"), bind->symbol, check->keycode, bind->eventStateMask);
            }
        }
        DEBUG(_("state=0x%x, cfg=\"%s\", sym=0x%x → keycodes%s (%d)"), bind->eventStateMask, bind->symbol, keysym, keycodes, numKeycodes);
        free(keycodes);
    }

out:
    xkb_state_unref(dummyState);
    xkb_state_unref(dummyStateNoShift);
    xkb_state_unref(dummyStateNumlock);
    xkb_state_unref(dummyStateNumlockNoShift);

    if (hasErrors) {
        config_start_config_error_nag_bar(gCurConfigPath, true);
    }
}

void key_binding_grab_all_keys(xcb_connection_t *conn)
{
    GWMBinding* bind = NULL;
    for (GList* ls1 = gBindings; ls1; ls1 = ls1->next) {
        if (bind->inputType != B_KEYBOARD) {
            continue;
        }

        if (!binding_in_current_group(bind))
            continue;

        /* The easy case: the user specified a keycode directly. */
        if (bind->keycode > 0) {
            grab_keycode_for_binding(conn, bind, bind->keycode);
            continue;
        }

        GWMBindingKeycode* bindingKeycode = NULL;
        for (GList* ls2 = bind->keycodesHead.head; ls2; ls2 = ls2->next) {
            bindingKeycode = ls2->data;
            const int keycode = bindingKeycode->keycode;
            const unsigned int mods = (bindingKeycode->modifiers & 0xFFFF);
            DEBUG(_("Binding %p Grabbing keycode %d with mods %d"), bind, keycode, mods);
            xcb_grab_key(conn, 0, gRoot, mods, keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
        }
    }
}

GWMBinding* key_binding_configure_binding(
        const char *bindType,
        const char *modifiers,
        const char *inputCode,
        const char *release,
        const char *border,
        const char *wholeWindow,
        const char *excludeTitleBar,
        const char *command,
        const char *modeName,
        bool pangoMarkup)
{
    GWMBinding *newBinding = g_malloc0(sizeof(GWMBinding));
    DEBUG(_("Binding %p bindtype %s, modifiers %s, input code %s, release %s"), newBinding, bindType, modifiers, inputCode, release);
    newBinding->release = (release != NULL ? B_UPON_KEYRELEASE : B_UPON_KEYPRESS);
    newBinding->border = (border != NULL);
    newBinding->wholeWindow = (wholeWindow != NULL);
    newBinding->excludeTitleBar = (excludeTitleBar != NULL);
    if (strcmp(bindType, "bindsym") == 0) {
        newBinding->inputType = (strncasecmp(inputCode, "button", (sizeof("button") - 1)) == 0 ? B_MOUSE : B_KEYBOARD);
        newBinding->symbol = g_strdup(inputCode);
    }
    else {
        long keycode;
        if (!util_parse_long(inputCode, &keycode, 10)) {
            ERROR(_("Could not parse \"%s\" as an input code, ignoring this binding."), inputCode);
            {
                free (newBinding);
                newBinding = NULL;
            }
            return NULL;
        }

        newBinding->keycode = keycode;
        newBinding->inputType = B_KEYBOARD;
    }

    int groupBitsSet = 0;
    newBinding->command = g_strdup(command);
    newBinding->eventStateMask = config_event_state_from_str(modifiers);
    if ((newBinding->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_1) {
        groupBitsSet++;
    }

    if ((newBinding->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_2) {
        groupBitsSet++;
    }

    if ((newBinding->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_3) {
        groupBitsSet++;
    }

    if ((newBinding->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_4) {
        groupBitsSet++;
    }

    if (groupBitsSet > 1) {
        ERROR(_("Keybinding has more than one Group specified, but your X server is always in precisely one group. The keybinding can never trigger."));
    }

    GWMConfigMode* mode = mode_from_name(modeName, pangoMarkup);

    mode->bindings = g_slist_append (mode->bindings, newBinding);
    g_queue_init (&(newBinding->keycodesHead));

    return newBinding;
}

void key_binding_regrab_all_buttons(xcb_connection_t *conn)
{
    g_autofree int* buttons = key_binding_get_buttons_to_grab();
    xcb_grab_server(conn);

    GWMContainer* con;
    TAILQ_FOREACH (con, &gAllContainer, allContainers) {
        if (con->window == NULL) {
            continue;
        }

        xcb_ungrab_button(conn, XCB_BUTTON_INDEX_ANY, con->window->id, XCB_BUTTON_MASK_ANY);
        xcb_gwm_grab_buttons(conn, con->window->id, buttons);
    }

    xcb_ungrab_server(conn);
}

void key_binding_recorder_bindings(void)
{
    GWMConfigMode* mode = NULL;
    for (GSList* ls = gConfigModes; ls; ls = ls->next) {
        const bool currentMode = (mode->bindings == gBindings);
        reorder_bindings_of_mode(mode);
        if (currentMode) {
            gBindings = mode->bindings;
        }
    }
}

void key_binding_free(GWMBinding *bind)
{
    if (bind == NULL) {
        return;
    }

    g_queue_clear_full (&(bind->keycodesHead.head), g_free);
    g_queue_init (&(bind->keycodesHead));
    g_free (bind->symbol); bind->symbol = NULL;
    g_free (bind->command); bind->command = NULL;
//    for (GList* ls = bind->keycodesHead.head; ls; ls = ls->next) {
//        GWMBindingKeycode* first = bind->keycodesHead.head->data; //TAILQ_FIRST(&(bind->keycodes_head));
//        TAILQ_REMOVE(&(bind->keycodes_head), first, keycodes);
//        FREE(first);
//    }
//    FREE(bind->symbol);
//    FREE(bind->command);
//    FREE(bind);
    g_free (bind);
}

int *key_binding_get_buttons_to_grab(void)
{
    return NULL;
}

void key_binding_switch_mode(const char *newMode)
{
    GWMConfigMode* mode = NULL;

    DEBUG(_("Switching to mode %s"), newMode);

    for (GList* ls1 = gConfigModes; ls1; ls1 = ls1->next) {
//    SLIST_FOREACH (mode, &modes, modes) {
        if (strcmp(mode->name, newMode) != 0) {
            continue;
        }

        key_binding_ungrab_all_keys(gConn);
        gBindings = mode->bindings;
        gCurrentBindingMode = mode->name;
        key_binding_translate_keysyms();
        key_binding_grab_all_keys(gConn);
        key_binding_regrab_all_buttons(gConn);

        /* Reset all B_UPON_KEYRELEASE_IGNORE_MODS bindings to avoid possibly
         * activating one of them. */
        GWMBinding* bind = NULL;
        for (GList* ls2 = gBindings; ls2; ls2 = ls2->next) {
//        TAILQ_FOREACH (bind, bindings, bindings) {
            if (bind->release == B_UPON_KEYRELEASE_IGNORE_MODS) {
                bind->release = B_UPON_KEYRELEASE;
            }
        }

        g_autofree char *eventMsg = g_strdup_printf("{\"change\":\"%s\", \"pango_markup\":%s}", mode->name, (mode->pangoMarkup ? "true" : "false"));


//        ipc_send_event("mode", I3_IPC_EVENT_MODE, event_msg);
//        FREE(event_msg);

        return;
    }

    ERROR("Mode not found");
}

void key_binding_check_for_duplicate_bindings(GWMConfigContext *context)
{
    GWMBinding* bind = NULL;
    GWMBinding* current = NULL;

    for (GList* ls1 = gBindings; ls1; ls1 = ls1->next) {
        current = ls1->data;
        for (GList* ls2 = gBindings; ls2; ls2 = ls2->next) {
            bind = ls2->data;
//    TAILQ_FOREACH (current, bindings, bindings) {
//        TAILQ_FOREACH (bind, bindings, bindings) {
            /* Abort when we reach the current keybinding, only check the
             * bindings before */
            if (bind == current) {
                break;
            }

            if (!binding_same_key(bind, current)) {
                continue;
            }

            context->hasErrors = true;
            if (current->keycode != 0) {
                ERROR(_("Duplicate keybinding in config file:\n  state mask 0x%x with keycode %d, command \"%s\""), current->eventStateMask, current->keycode, current->command);
            }
            else {
                ERROR(_("Duplicate keybinding in config file:\n  state mask 0x%x with keysym %s, command \"%s\""), current->eventStateMask, current->symbol, current->command);
            }
        }
    }
}

GWMBinding *key_binding_get_binding_from_xcb_event(xcb_generic_event_t *event)
{
    return NULL;
}

void key_binding_ungrab_all_keys(xcb_connection_t *conn)
{
    DEBUG(_("Ungrab all keys"));
    xcb_ungrab_key (conn, XCB_GRAB_ANY, gRoot, XCB_BUTTON_MASK_ANY);
}

static int fill_rmlvo_from_root(struct xkb_rule_names *xkbNames)
{
    size_t contentMaxWords = 256;
    xcb_intern_atom_reply_t *atomReply;

    atomReply = xcb_intern_atom_reply(gConn, xcb_intern_atom(gConn, 0, strlen("_XKB_RULES_NAMES"), "_XKB_RULES_NAMES"), NULL);
    if (atomReply == NULL) {
        return -1;
    }

    xcb_get_property_cookie_t propCookie;
    xcb_get_property_reply_t *propReply;
    propCookie = xcb_get_property_unchecked(gConn, false, gRoot, atomReply->atom, XCB_GET_PROPERTY_TYPE_ANY, 0, contentMaxWords);
    propReply = xcb_get_property_reply(gConn, propCookie, NULL);
    if (NULL == propReply) {
        free(atomReply);
        return -1;
    }

    if (xcb_get_property_value_length(propReply) > 0 && propReply->bytes_after > 0) {
        contentMaxWords += ceil(propReply->bytes_after / 4.0);
        free(propReply);
        propCookie = xcb_get_property_unchecked(gConn, false, gRoot, atomReply->atom, XCB_GET_PROPERTY_TYPE_ANY, 0, contentMaxWords);
        propReply = xcb_get_property_reply(gConn, propCookie, NULL);
        if (NULL == propReply) {
            free(atomReply);
            return -1;
        }
    }

    if (xcb_get_property_value_length(propReply) == 0) {
        free(atomReply);
        free(propReply);
        return -1;
    }

    const char* walk = (const char *)xcb_get_property_value(propReply);
    int remaining = xcb_get_property_value_length(propReply);
    for (int i = 0; i < 5 && remaining > 0; i++) {
        const int len = (int) strnlen (walk, remaining);
        switch (i) {
            case 0: {
                g_snprintf((char **)&(xkbNames->rules), len, "%.*s", walk);
                break;
            }
            case 1: {
                g_snprintf((char **)&(xkbNames->model), len, "%.*s", walk);
                break;
            }
            case 2: {
                g_snprintf((char **)&(xkbNames->layout), len, "%.*s", walk);
                break;
            }
            case 3: {
                g_snprintf((char **)&(xkbNames->variant), len, "%.*s", walk);
                break;
            }
            case 4: {
                g_snprintf((char **)&(xkbNames->options), len, "%.*s", walk);
                break;
            }
            default:{
                break;
            }
        }
        DEBUG(_("component %d of _XKB_RULES_NAMES is \"%.*s\""), i, len, walk);
        walk += (len + 1);
        remaining -= (len + 1);
    }

    free(atomReply);
    free(propReply);

    return 0;
}

static GWMConfigMode* mode_from_name(const char *name, bool pangoMarkup)
{
    GWMConfigMode* mode = NULL;

    /* Try to find the mode in the list of modes and return it */
//    SLIST_FOREACH (mode, &modes, modes) {
    for (GSList* ls = gConfigModes; ls; ls = ls->next) {
        mode = ls->data;
        if (strcmp(mode->name, name) == 0) {
            return mode;
        }
    }

    /* If the mode was not found, create a new one */
    mode = g_malloc0 (sizeof(GWMConfigMode));
    g_return_val_if_fail(mode, NULL);

    mode->name = g_strdup(name);
    mode->pangoMarkup = pangoMarkup;
    mode->bindings = NULL; //g_malloc0 (sizeof(struct bindings_head));
    gConfigModes = g_slist_append(gConfigModes, mode);

    return mode;
}

static void add_keycode_if_matches(struct xkb_keymap *keymap, xkb_keycode_t key, void *data)
{
    const struct resolve *resolving = data;
    struct xkb_state *numlockState = resolving->xkbStateNumlock;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(resolving->xkbState, key);
    if (sym != resolving->keysym) {
        const xkb_layout_index_t layout = xkb_state_key_get_layout(resolving->xkbState, key);
        if (layout == XKB_LAYOUT_INVALID) {
            return;
        }

        if (xkb_state_key_get_level(resolving->xkbState, key, layout) > 1) {
            return;
        }

        if (sym >= XKB_KEY_KP_Space && sym <= XKB_KEY_KP_Equal) {
            return;
        }

        numlockState = resolving->xkbStateNumlockNoShift;
        sym = xkb_state_key_get_one_sym(resolving->xkbStateNoShift, key);
        if (sym != resolving->keysym)
            return;
    }
    GWMBinding* bind = resolving->bind;

    ADD_TRANSLATED_KEY(key, bind->eventStateMask);

    /* Also bind the key with active CapsLock */
    ADD_TRANSLATED_KEY(key, bind->eventStateMask | XCB_MOD_MASK_LOCK);

    /* If this binding is not explicitly for NumLock, check whether we need to
     * add a fallback. */
    if ((bind->eventStateMask & gXCBNumLockMask) != gXCBNumLockMask) {
        /* Check whether the keycode results in the same keysym when NumLock is
         * active. If so, grab the key with NumLock as well, so that users don’t
         * need to duplicate every key binding with an additional Mod2 specified.
         */
        xkb_keysym_t symNumlock = xkb_state_key_get_one_sym(numlockState, key);
        if (symNumlock == resolving->keysym) {
            /* Also bind the key with active NumLock */
            ADD_TRANSLATED_KEY(key, bind->eventStateMask | gXCBNumLockMask);

            /* Also bind the key with active NumLock+CapsLock */
            ADD_TRANSLATED_KEY(key, bind->eventStateMask | gXCBNumLockMask | XCB_MOD_MASK_LOCK);
        }
        else {
            DEBUG(_("Skipping automatic numlock fallback, key %d resolves to 0x%x with numlock"), key, symNumlock);
        }
    }
}

static bool binding_in_current_group(const GWMBinding *bind)
{
    /* If no bits are set, the binding should be installed in every group. */
    if ((bind->eventStateMask >> 16) == GWM_XKB_GROUP_MASK_ANY) {
        return true;
    }

    switch (gXKBCurrentGroup) {
        case XCB_XKB_GROUP_1: {
            return ((bind->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_1);
        }
        case XCB_XKB_GROUP_2: {
            return ((bind->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_2);
        }
        case XCB_XKB_GROUP_3: {
            return ((bind->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_3);
        }
        case XCB_XKB_GROUP_4: {
            return ((bind->eventStateMask >> 16) & GWM_XKB_GROUP_MASK_4);
        }
        default: {
            ERROR(_("BUG: xkb_current_group (= %d) outside of [XCB_XKB_GROUP_1..XCB_XKB_GROUP_4]"), gXKBCurrentGroup);
            return false;
        }
    }
}

static void grab_keycode_for_binding(xcb_connection_t* conn, GWMBinding* bind, uint32_t keycode)
{
    /* Grab the key in all combinations */
#define GRAB_KEY(modifier)                                                                          \
    do {                                                                                            \
        xcb_grab_key(gConn, 0, gRoot, modifier, keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);  \
    } while (0)
    const unsigned int mods = (bind->eventStateMask & 0xFFFF);
    DEBUG(_("Binding %p Grabbing keycode %d with event state mask 0x%x (mods 0x%x)"), bind, keycode, bind->eventStateMask, mods);
    GRAB_KEY(mods);
    /* Also bind the key with active NumLock */
    GRAB_KEY(mods | gXCBNumLockMask);
    /* Also bind the key with active CapsLock */
    GRAB_KEY(mods | XCB_MOD_MASK_LOCK);
    /* Also bind the key with active NumLock+CapsLock */
    GRAB_KEY(mods | gXCBNumLockMask | XCB_MOD_MASK_LOCK);
}

static void reorder_bindings_of_mode(GWMConfigMode *mode)
{
    /* Copy the bindings into an array, so that we can use qsort(3). */
    int n = 0;
    GWMBinding* current = NULL;
    for (GList* ls = mode->bindings; ls; ls = ls->next) {
        n++;
    }

    GWMBinding **tmp = g_malloc0(n * sizeof(GWMBinding*));
    n = 0;
    for (GList* ls = mode->bindings; ls; ls = ls->next) {
        current = ls->data;
        tmp[n++] = current;
    }

    qsort(tmp, n, sizeof(GWMBinding*), reorder_binding_cmp);

    GList* reordered = g_malloc0 (sizeof (GList));
//    bindings_head *reordered = g_malloc0(sizeof(GList));
//    TAILQ_INIT(reordered);
    for (int i = 0; i < n; i++) {
        current = tmp[i];
        mode->bindings = g_list_remove (mode->bindings, current);
//        TAILQ_REMOVE(mode->bindings, current, bindings);
//        TAILQ_INSERT_TAIL(reordered, current, bindings);
        reordered = g_list_append (reordered, current);
    }
    free(tmp);

//    assert(TAILQ_EMPTY(mode->bindings));

    /* Free the old bindings_head, which is now empty. */
    free(mode->bindings);

    mode->bindings = reordered;
}

static int reorder_binding_cmp(const void *a, const void *b)
{
    GWMBinding *first = *((GWMBinding**)a);
    GWMBinding *second = *((GWMBinding**)b);
    if (first->eventStateMask < second->eventStateMask) {
        return 1;
    } else if (first->eventStateMask == second->eventStateMask) {
        return 0;
    } else {
        return -1;
    }
}

static bool binding_same_key(GWMBinding* a, GWMBinding* b)
{
    /* Check if the input types are different */
    if (a->inputType != b->inputType) {
        return false;
    }

    /* Check if one is using keysym while the other is using bindsym. */
    if ((a->symbol == NULL && b->symbol != NULL)
        || (a->symbol != NULL && b->symbol == NULL)) {
        return false;
    }

    /* If a is NULL, b has to be NULL, too (see previous conditional).
     * If the keycodes differ, it can't be a duplicate. */
    if (a->symbol != NULL && g_strcasecmp(a->symbol, b->symbol) != 0) {
        return false;
    }

    /* Check if the keycodes or modifiers are different. If so, they
     * can't be duplicate */
    if (a->keycode != b->keycode || a->eventStateMask != b->eventStateMask || a->release != b->release) {
        return false;
    }

    return true;
}