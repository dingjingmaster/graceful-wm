//
// Created by dingjing on 23-11-23.
//

#include "utils.h"

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

uint32_t util_get_mod_mask_for(uint32_t keySym, xcb_key_symbols_t *symbols)
{
    return 0;
}


