//
// Created by dingjing on 23-11-23.
//

#include "utils.h"

#include "val.h"


xcb_visualtype_t *get_visual_type(xcb_screen_t *screen)
{
    for (xcb_depth_iterator_t it = xcb_screen_allowed_depths_iterator(screen); it.rem; xcb_depth_next(&it)) {
        for (struct xcb_visualtype_iterator_t vit = xcb_depth_visuals_iterator(it.data); vit.rem; xcb_visualtype_next(&vit)) {
            if (screen->root_visual == vit.data->visual_id) {
                return vit.data;
            }
        }
    }

    return NULL;
}

uint32_t util_aio_get_mod_mask_for(uint32_t keySym, xcb_key_symbols_t *symbols)
{
    xcb_get_modifier_mapping_cookie_t   cookie;
    xcb_get_modifier_mapping_reply_t*   modMapR = NULL;

    xcb_flush(gConn);

    cookie = xcb_get_modifier_mapping(gConn);
    if (!(modMapR = xcb_get_modifier_mapping_reply(gConn, cookie, NULL))) {
        return 0;
    }

    uint32_t result = util_get_mod_mask_for(keySym, symbols, modMapR);

    free(modMapR);

    return result;
}

uint32_t util_get_mod_mask_for(uint32_t keySym, xcb_key_symbols_t *symbols, xcb_get_modifier_mapping_reply_t *modMapReply) {
    xcb_keycode_t modCode;
    xcb_keycode_t *codes, *modMap;

    modMap = xcb_get_modifier_mapping_keycodes (modMapReply);

    if (!(codes = xcb_key_symbols_get_keycode (symbols, keySym))) {
        return 0;
    }

    for (int mod = 0; mod < 8; mod++) {
        for (int j = 0; j < modMapReply->keycodes_per_modifier; j++) {
            modCode = modMap[(mod * modMapReply->keycodes_per_modifier) + j];
            for (xcb_keycode_t *code = codes; *code; code++) {
                if (*code != modCode) {
                    continue;
                }
                free (codes);
                return (1 << mod);
            }
        }
    }

    return 0;
}


