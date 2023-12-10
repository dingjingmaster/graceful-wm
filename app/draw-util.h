//
// Created by dingjing on 23-11-27.
//

#ifndef GRACEFUL_WM_DRAW_UTIL_H
#define GRACEFUL_WM_DRAW_UTIL_H
#include "types.h"


GWMColor draw_util_hex_to_color(const char *color);
uint32_t draw_util_get_color_pixel(const char *hex);
uint16_t draw_util_get_visual_depth(xcb_visualid_t visualID);
void draw_util_clear_surface(GWMSurface* surface, GWMColor color);
void draw_util_surface_free(xcb_connection_t* conn, GWMSurface* surface);
void draw_util_surface_set_size(GWMSurface* surface, int width, int height);
void draw_util_set_font_colors(xcb_gcontext_t gc, GWMColor foreground, GWMColor background);
void draw_util_rectangle(GWMSurface* surface, GWMColor color, double x, double y, double w, double h);
void draw_util_text(const char* text, GWMSurface* surface, GWMColor fgColor, GWMColor bgColor, int x, int y, int maxWidth);
void draw_util_draw_text(const char* text, xcb_drawable_t drawable, xcb_gcontext_t gc, cairo_surface_t *surface, int x, int y, int maxWidth);
void draw_util_surface_init(xcb_connection_t *conn, GWMSurface* surface, xcb_drawable_t drawable, xcb_visualtype_t *visual, int width, int height);
void draw_util_copy_surface(GWMSurface* src, GWMSurface* destin, double srcX, double srcY, double destinX, double destinY, double width, double height);

#endif //GRACEFUL_WM_DRAW_UTIL_H
