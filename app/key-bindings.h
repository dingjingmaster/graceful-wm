//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_KEY_BINDINGS_H
#define GRACEFUL_WM_KEY_BINDINGS_H
#include <stdbool.h>
#include <xcb/xcb.h>

bool key_binding_load_keymap(void);
void key_binding_translate_keymaps(void);
void key_binding_grab_all_keys(xcb_connection_t* conn);


#endif //GRACEFUL_WM_KEY_BINDINGS_H
