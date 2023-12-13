//
// Created by dingjing on 23-12-10.
//

#ifndef GRACEFUL_WM_FONT_H
#define GRACEFUL_WM_FONT_H
#include "types.h"

bool    font_is_pango(void);
void    font_free_font(void);
void    font_set_font(GWMFont *font);
int     font_predict_text_width(const char* text);
GWMFont font_load_font(const char *pattern, bool fallback);
void    font_set_font_colors(xcb_gcontext_t gc, GWMColor foreground, GWMColor background);
void    font_draw_text(const char*text, xcb_drawable_t drawable, xcb_gcontext_t gc, cairo_surface_t *surface, int x, int y, int maxWidth);


#endif //GRACEFUL_WM_FONT_H
