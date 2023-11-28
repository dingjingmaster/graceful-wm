//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_KEY_BINDINGS_H
#define GRACEFUL_WM_KEY_BINDINGS_H
#include <stdbool.h>
#include <xcb/xcb.h>

#include "types.h"


extern pid_t gCommandErrorNagBarPid;
extern const char* DEFAULT_BINDING_MODE;

bool key_binding_load_keymap(void);
void key_binding_translate_keysyms(void);
void key_binding_recorder_bindings(void);
void key_binding_free (GWMBinding* bind);
int* key_binding_get_buttons_to_grab(void);
void key_binding_switch_mode(const char* newMode);
void key_binding_grab_all_keys(xcb_connection_t* conn);
void key_binding_ungrab_all_keys(xcb_connection_t* conn);
void key_binding_regrab_all_buttons(xcb_connection_t* conn);
void key_binding_check_for_duplicate_bindings(GWMConfigContext* context);
GWMBinding* key_binding_get_binding_from_xcb_event (xcb_generic_event_t* event);
GWMBinding* key_binding_configure_binding(const char* bindType, const char* modifiers, const char* inputCode, const char* release, const char* border, const char* wholeWindow, const char* excludeTitleBar, const char* command, const char* modeName, bool pangoMarkup);


#endif //GRACEFUL_WM_KEY_BINDINGS_H
