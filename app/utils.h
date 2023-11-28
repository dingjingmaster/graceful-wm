//
// Created by dingjing on 23-11-23.
//

#ifndef GRACEFUL_WM_UTILS_H
#define GRACEFUL_WM_UTILS_H
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <xcb/shape.h>
#include <xcb/bigreq.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_xrm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xinerama.h>
#include <xcb/xcb_keysyms.h>


xcb_visualtype_t* get_visual_type (xcb_screen_t* screen);

bool util_parse_long(const char *str, long *out, int base);
uint32_t util_aio_get_mod_mask_for (uint32_t keySym, xcb_key_symbols_t* symbols);
uint32_t util_get_mod_mask_for (uint32_t keySym, xcb_key_symbols_t* symbols, xcb_get_modifier_mapping_reply_t* modMapReply);

#endif //GRACEFUL_WM_UTILS_H
