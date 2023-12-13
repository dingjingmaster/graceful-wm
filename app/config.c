//
// Created by dingjing on 23-11-28.
//

#include "config.h"

#include "val.h"
#include "log.h"
#include "tree.h"
#include "draw-util.h"
#include "dpi.h"
#include "workspace.h"
#include "key-bindings.h"


GWMEventStateMask config_event_state_from_str(const char *str)
{
    GWMEventStateMask result = 0;

    if (str == NULL) {
        return result;
    }

    if (strstr(str, "Mod1") != NULL) {
        result |= XCB_KEY_BUT_MASK_MOD_1;
    }

    if (strstr(str, "Mod2") != NULL) {
        result |= XCB_KEY_BUT_MASK_MOD_2;
    }

    if (strstr(str, "Mod3") != NULL) {
        result |= XCB_KEY_BUT_MASK_MOD_3;
    }

    if (strstr(str, "Mod4") != NULL) {
        result |= XCB_KEY_BUT_MASK_MOD_4;
    }

    if (strstr(str, "Mod5") != NULL) {
        result |= XCB_KEY_BUT_MASK_MOD_5;
    }

    if (strstr(str, "Control") != NULL || strstr(str, "Ctrl") != NULL) {
        result |= XCB_KEY_BUT_MASK_CONTROL;
    }

    if (strstr(str, "Shift") != NULL) {
        result |= XCB_KEY_BUT_MASK_SHIFT;
    }

    if (strstr(str, "Group1") != NULL) {
        result |= (GWM_XKB_GROUP_MASK_1 << 16);
    }

    if (strstr(str, "Group2") != NULL || strstr(str, "Mode_switch") != NULL) {
        result |= (GWM_XKB_GROUP_MASK_2 << 16);
    }

    if (strstr(str, "Group3") != NULL) {
        result |= (GWM_XKB_GROUP_MASK_3 << 16);
    }

    if (strstr(str, "Group4") != NULL) {
        result |= (GWM_XKB_GROUP_MASK_4 << 16);
    }

    return result;
}

void config_start_config_error_nag_bar(const char *configPath, bool has_errors)
{

}

bool config_load_configuration(const char *overrideConfigfile)
{
    bool ret = true;

    gBindings = g_malloc0 (sizeof(GWMBinding));
    EXIT_IF_MEM_IS_NULL(gBindings);

    memset (&gConfig, 0, sizeof (GWMConfig));

#define INIT_COLOR(x, cborder, cbackground, ctext, cindicator) \
    do {                                                       \
        x.border = draw_util_hex_to_color(cborder);            \
        x.background = draw_util_hex_to_color(cbackground);    \
        x.text = draw_util_hex_to_color(ctext);                \
        x.indicator = draw_util_hex_to_color(cindicator);      \
        x.childBorder = draw_util_hex_to_color(cbackground);   \
    } while (0)

    gConfig.client.background = draw_util_hex_to_color("#000000");
    INIT_COLOR(gConfig.client.focused, "#4c7899", "#285577", "#ffffff", "#2e9ef4");
    INIT_COLOR(gConfig.client.focused_inactive, "#333333", "#5f676a", "#ffffff", "#484e50");
    INIT_COLOR(gConfig.client.unfocused, "#333333", "#222222", "#888888", "#292d2e");
    INIT_COLOR(gConfig.client.urgent, "#2f343a", "#900000", "#ffffff", "#900000");
    gConfig.client.gotFocusedTabTitle = false;

    /* border and indicator color are ignored for placeholder contents */
    INIT_COLOR(gConfig.client.placeholder, "#000000", "#0c0c0c", "#ffffff", "#000000");

    /* the last argument (indicator color) is ignored for bar colors */
    INIT_COLOR(gConfig.bar.focused, "#4c7899", "#285577", "#ffffff", "#000000");
    INIT_COLOR(gConfig.bar.unfocused, "#333333", "#222222", "#888888", "#000000");
    INIT_COLOR(gConfig.bar.urgent, "#2f343a", "#900000", "#ffffff", "#000000");

    gConfig.showMarks = true;

    gConfig.defaultBorder = BS_NORMAL;
    gConfig.defaultFloatingBorder = BS_NORMAL;
    gConfig.defaultBorderWidth = dpi_logical_px(2);
    gConfig.defaultFloatingBorderWidth = dpi_logical_px(2);
    gConfig.defaultOrientation = NO_ORIENTATION;                // Set default_orientation to NO_ORIENTATION for auto orientation.
    gConfig.gaps.inner = 0;
    gConfig.gaps.top = 0;
    gConfig.gaps.right = 0;
    gConfig.gaps.bottom = 0;
    gConfig.gaps.left = 0;

    /* Set default urgency reset delay to 500ms */
    if (gConfig.workspaceUrgencyTimer == 0) {
        gConfig.workspaceUrgencyTimer = 0.5;
    }

    gConfig.focusWrapping = FOCUS_WRAPPING_ON;
    gConfig.tilingDrag = TILING_DRAG_MODIFIER;

    if (overrideConfigfile) {
        FREE(gCurrentConfigPath);
        gCurrentConfigPath = g_strdup(overrideConfigfile);
        if (gCurrentConfigPath == NULL) {
            DIE("Unable to find the configuration file (looked at $XDG_CONFIG_HOME/gwm/config, ~/.gwm/config, $XDG_CONFIG_DIRS/gwm/config and /etc/gwm/config)");
        }

        // resolve config path

        // @FIXME:// parse config file
        INFO("Parsing configfile %s", gCurrentConfigPath);
        // ret = true?
    }

    workspace_extract_workspace_names_from_bindings();
    key_binding_reorder_bindings();

//    if (gConfig.font.type == FONT_TYPE_NONE) {
//        ELOG("You did not specify required configuration option \"font\"");
//        config.font = load_font("fixed", true);
//        set_font(&config.font);
//    }


//    if (load_type == C_RELOAD) {
//
//        translate_keysyms();
//        grab_all_keys(conn);
//        regrab_all_buttons(conn);
//        gaps_reapply_workspace_assignments();
//
//        /* Redraw the currently visible decorations on reload, so that the
//         * possibly new drawing parameters changed. */
//        tree_render();
//    }

    return ret;
}

void config_ungrab_all_keys(xcb_connection_t *conn)
{
    DEBUG("Ungrabbing all keys");
    xcb_ungrab_key(conn, XCB_GRAB_ANY, gRoot, XCB_BUTTON_MASK_ANY);
}

