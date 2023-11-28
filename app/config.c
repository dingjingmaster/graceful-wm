//
// Created by dingjing on 23-11-28.
//

#include "config.h"

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
