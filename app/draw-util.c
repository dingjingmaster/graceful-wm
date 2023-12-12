//
// Created by dingjing on 23-11-27.
//

#include "draw-util.h"

#include <cairo.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <cairo-xcb.h>
#include <xcb/xcb_aux.h>
#include <cairo/cairo-xcb.h>
#include <pango/pangocairo.h>

#include "val.h"
#include "log.h"
#include "dpi.h"

static const GWMFont*               gsSavedFont = NULL;
static xcb_visualtype_t*            gsRootVisualType;
static double                       gsPangoFontRed;
static double                       gsPangoFontGreen;
static double                       gsPangoFontBlue;
static double                       gsPangoFontAlpha;


static bool surface_initialized(GWMSurface* surface);
static PangoLayout* create_layout_with_dpi(cairo_t *cr);
static void util_set_source_color(GWMSurface* surface, GWMColor color);
static int xcb_query_text_width(const xcb_char2b_t* text, size_t textLen);
static void draw_util_set_source_color(GWMSurface* surface, GWMColor color);
static int predict_text_width_xcb(const xcb_char2b_t *input, size_t textLen);
static xcb_gcontext_t get_gc(xcb_connection_t *conn, uint8_t depth, xcb_drawable_t drawable, bool* shouldFree);
static void draw_text_xcb(const xcb_char2b_t* text, size_t textLen, xcb_drawable_t drawable, xcb_gcontext_t gc, int x, int y);
static void draw_text_pango(const char *text, size_t textLen, xcb_drawable_t drawable, cairo_surface_t *surface, int x, int y, int maxWidth, bool pangoMarkup);


void draw_util_surface_init(xcb_connection_t *conn, GWMSurface* surface, xcb_drawable_t drawable, xcb_visualtype_t *visual, int width, int height)
{
    surface->id = drawable;
    surface->width = width;
    surface->height = height;

    if (visual == NULL) {
        visual = gVisualType;
    }

    surface->gc = get_gc(conn, draw_util_get_visual_depth(visual->visual_id), drawable, &surface->ownsGC);
    surface->surface = cairo_xcb_surface_create(conn, surface->id, visual, width, height);
    surface->cairo = cairo_create(surface->surface);
}

void draw_util_surface_free(xcb_connection_t* conn, GWMSurface* surface)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    if (surface->cairo) {
        status = cairo_status(surface->cairo);
    }
    if (status != CAIRO_STATUS_SUCCESS) {
        INFO("Found cairo context in an error status while freeing, error %d is %s", status, cairo_status_to_string(status));
    }

    if (surface->ownsGC) {
        xcb_free_gc(conn, surface->gc);
    }
    cairo_surface_destroy(surface->surface);
    cairo_destroy(surface->cairo);

    /* We need to explicitly set these to NULL to avoid assertion errors in
     * cairo when calling this multiple times. This can happen, for example,
     * when setting the border of a window to none and then closing it. */
    surface->surface = NULL;
    surface->cairo = NULL;
}

void draw_util_surface_set_size(GWMSurface* surface, int width, int height)
{
    surface->width = width;
    surface->height = height;
    cairo_xcb_surface_set_size(surface->surface, width, height);
}

GWMColor draw_util_hex_to_color(const char *color)
{
    if (strlen(color) < 6 || color[0] != '#') {
        ERROR("Could not parse color: %s", color);
        return draw_util_hex_to_color("#A9A9A9");
    }

    char alpha[2];
    if (strlen(color) == strlen("#rrggbbaa")) {
        alpha[0] = color[7];
        alpha[1] = color[8];
    } else {
        alpha[0] = alpha[1] = 'F';
    }

    char groups[4][3] = {
        {color[1], color[2], '\0'},
        {color[3], color[4], '\0'},
        {color[5], color[6], '\0'},
        {alpha[0], alpha[1], '\0'}};

    return (GWMColor) {
        .red = strtol(groups[0], NULL, 16) / 255.0,
        .green = strtol(groups[1], NULL, 16) / 255.0,
        .blue = strtol(groups[2], NULL, 16) / 255.0,
        .alpha = strtol(groups[3], NULL, 16) / 255.0,
        .colorPixel = draw_util_get_color_pixel(color)};
}

uint32_t draw_util_get_color_pixel(const char* hex)
{
    char alpha[2];
    if (strlen(hex) == strlen("#rrggbbaa")) {
        alpha[0] = hex[7];
        alpha[1] = hex[8];
    }
    else {
        alpha[0] = alpha[1] = 'F';
    }

    char strGroups[4][3] = {
        {hex[1], hex[2], '\0'},
        {hex[3], hex[4], '\0'},
        {hex[5], hex[6], '\0'},
        {alpha[0], alpha[1], '\0'}};
    uint8_t r = strtol(strGroups[0], NULL, 16);
    uint8_t g = strtol(strGroups[1], NULL, 16);
    uint8_t b = strtol(strGroups[2], NULL, 16);
    uint8_t a = strtol(strGroups[3], NULL, 16);

    if (gRootScreen == NULL || gRootScreen->root_depth == 24 || gRootScreen->root_depth == 32) {
        return (a << 24) | (r << 16 | g << 8 | b);
    }

    GWMColorPixel *colorPixel;
    SLIST_FOREACH (colorPixel, &(gColorPixels), colorPixels) {
        if (strcmp(colorPixel->hex, hex) == 0) {
            return colorPixel->pixel;
        }
    }

#define RGB_8_TO_16(i) (65535 * ((i)&0xFF) / 255)
    int r16 = RGB_8_TO_16(r);
    int g16 = RGB_8_TO_16(g);
    int b16 = RGB_8_TO_16(b);

    xcb_alloc_color_reply_t *reply;

    reply = xcb_alloc_color_reply(gConn, xcb_alloc_color(gConn, gRootScreen->default_colormap, r16, g16, b16), NULL);
    if (!reply) {
        INFO("Could not allocate color");
        exit(1);
    }

    uint32_t pixel = reply->pixel;
    free(reply);

    /* Store the result in the cache */
    GWMColorPixel* cachePixel = g_malloc0(sizeof(GWMColorPixel));

    strncpy(cachePixel->hex, hex, 7);
    cachePixel->hex[7] = '\0';
    cachePixel->pixel = pixel;

    SLIST_INSERT_HEAD(&(gColorPixels), cachePixel, colorPixels);

    return pixel;
}

void draw_util_set_font_colors(xcb_gcontext_t gc, GWMColor foreground, GWMColor background)
{
    g_assert(gsSavedFont != NULL);

    switch (gsSavedFont->type) {
        case FONT_TYPE_NONE:
            /* Nothing to do */
            break;
        case FONT_TYPE_XCB: {
            /* Change the font and colors in the GC */
            uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
            uint32_t values[] = {foreground.colorPixel, background.colorPixel, gsSavedFont->specific.xcb.id};
            xcb_change_gc(gConn, gc, mask, values);
            break;
        }
        case FONT_TYPE_PANGO:
            /* Save the foreground font */
            gsPangoFontRed = foreground.red;
            gsPangoFontGreen = foreground.green;
            gsPangoFontBlue = foreground.blue;
            gsPangoFontAlpha = foreground.alpha;
            break;
    }
}

void draw_util_text(const char* text, GWMSurface* surface, GWMColor fgColor, GWMColor bgColor, int x, int y, int max_width)
{
    if (!surface_initialized(surface)) {
        return;
    }

    /* Flush any changes before we draw the text as this might use XCB directly. */
    CAIRO_SURFACE_FLUSH(surface->surface);

    draw_util_set_font_colors(surface->gc, fgColor, bgColor);
    draw_util_draw_text(text, surface->id, surface->gc, surface->surface, x, y, max_width);

    /* Notify cairo that we (possibly) used another way to draw on the surface. */
    cairo_surface_mark_dirty(surface->surface);
}

void draw_util_draw_text(const char* text, xcb_drawable_t drawable, xcb_gcontext_t gc, cairo_surface_t *surface, int x, int y, int maxWidth)
{
    g_assert(gsSavedFont != NULL);

    switch (gsSavedFont->type) {
        case FONT_TYPE_NONE:
            /* Nothing to do */
            return;
        case FONT_TYPE_XCB:
            draw_text_xcb(text, strlen(text), drawable, gc, x, y);
            break;
        case FONT_TYPE_PANGO:
            /* Render the text using Pango */
            draw_text_pango(text, strlen(text), drawable, surface, x, y, maxWidth, true);
            return;
    }
}

void draw_util_copy_surface(GWMSurface *src, GWMSurface *destin, double srcX, double srcY, double destinX, double destinY, double width, double height)
{

}

uint16_t draw_util_get_visual_depth(xcb_visualid_t visualID)
{
    xcb_depth_iterator_t depth_iter;

    depth_iter = xcb_screen_allowed_depths_iterator(gRootScreen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        xcb_visualtype_iterator_t visual_iter;

        visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
        for (; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
            if (visualID == visual_iter.data->visual_id) {
                return depth_iter.data->depth;
            }
        }
    }

    return 0;
}

void draw_util_rectangle(GWMSurface* surface, GWMColor color, double x, double y, double w, double h)
{
    if (!surface_initialized(surface)) {
        return;
    }

    cairo_save(surface->cairo);

    cairo_set_operator(surface->cairo, CAIRO_OPERATOR_SOURCE);
    draw_util_set_source_color(surface, color);

    cairo_rectangle(surface->cairo, x, y, w, h);
    cairo_fill(surface->cairo);

    CAIRO_SURFACE_FLUSH(surface->surface);

    cairo_restore(surface->cairo);
}

void draw_util_clear_surface(GWMSurface* surface, GWMColor color)
{
    if (!surface_initialized(surface)) {
        return;
    }

    cairo_save(surface->cairo);
    cairo_set_operator(surface->cairo, CAIRO_OPERATOR_SOURCE);
    draw_util_set_source_color(surface, color);
    cairo_paint(surface->cairo);

    CAIRO_SURFACE_FLUSH(surface->surface);

    cairo_restore(surface->cairo);
}


static bool surface_initialized(GWMSurface* surface)
{
    if (surface->id == XCB_NONE) {
        ERROR("Surface %p is not initialized, skipping drawing.", surface);
        return false;
    }
    return true;
}

static xcb_gcontext_t get_gc(xcb_connection_t *conn, uint8_t depth, xcb_drawable_t drawable, bool* shouldFree)
{
    static struct {
        uint8_t depth;
        xcb_gcontext_t gc;
    } gc_cache[2] = {
        0,
    };

    size_t index = 0;
    bool cache = false;

    *shouldFree = false;
    for (; index < sizeof(gc_cache) / sizeof(gc_cache[0]); index++) {
        if (gc_cache[index].depth == depth) {
            return gc_cache[index].gc;
        }
        if (gc_cache[index].depth == 0) {
            cache = true;
            break;
        }
    }

    xcb_gcontext_t gc = xcb_generate_id(conn);
    /* The drawable is only used to get the root and depth, thus the GC is not
     * tied to the drawable and it can be re-used with different drawables. */
    xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(conn, gc, drawable, 0, NULL);

    xcb_generic_error_t *error = xcb_request_check(conn, gc_cookie);
    if (error != NULL) {
        ERROR("Could not create graphical context. Error code: %d. Please report this bug.", error->error_code);
        free(error);
        return gc;
    }

    if (cache) {
        gc_cache[index].depth = depth;
        gc_cache[index].gc = gc;
    }
    else {
        *shouldFree = true;
    }

    return gc;
}

static void util_set_source_color(GWMSurface* surface, GWMColor color)
{
    if (!surface_initialized(surface)) {
        return;
    }

    cairo_set_source_rgba(surface->cairo, color.red, color.green, color.blue, color.alpha);
}

static void draw_text_xcb(const xcb_char2b_t* text, size_t textLen, xcb_drawable_t drawable, xcb_gcontext_t gc, int x, int y)
{
    /* X11 coordinates for fonts start at the baseline */
    int pos_y = y + gsSavedFont->specific.xcb.info->font_ascent;

    /* The X11 protocol limits text drawing to 255 chars, so we may need
     * multiple calls */
    int offset = 0;
    for (;;) {
        /* Calculate the size of this chunk */
        int chunk_size = (textLen > 255 ? 255 : textLen);
        const xcb_char2b_t *chunk = text + offset;

        /* Draw it */
        xcb_image_text_16(gConn, chunk_size, drawable, gc, x, pos_y, chunk);

        /* Advance the offset and length of the text to draw */
        offset += chunk_size;
        textLen -= chunk_size;

        /* Check if we're done */
        if (textLen == 0) {
            break;
        }

        /* Advance pos_x based on the predicted text width */
        x += predict_text_width_xcb(chunk, chunk_size);
    }
}

static int predict_text_width_xcb(const xcb_char2b_t *input, size_t textLen)
{
    if (textLen == 0) {
        return 0;
    }

    int width;
    if (gsSavedFont->specific.xcb.table == NULL) {
        /* If we don't have a font table, fall back to querying the server */
        width = xcb_query_text_width(input, textLen);
    }
    else {
        /* Save some pointers for convenience */
        xcb_query_font_reply_t *font_info = gsSavedFont->specific.xcb.info;
        xcb_charinfo_t *font_table = gsSavedFont->specific.xcb.table;

        /* Calculate the width using the font table */
        width = 0;
        for (size_t i = 0; i < textLen; i++) {
            xcb_charinfo_t *info;
            int row = input[i].byte1;
            int col = input[i].byte2;

            if (row < font_info->min_byte1 ||
                row > font_info->max_byte1 ||
                col < font_info->min_char_or_byte2 ||
                col > font_info->max_char_or_byte2)
                continue;

            /* Don't you ask me, how this one works… (Merovius) */
            info = &font_table[((row - font_info->min_byte1) * (font_info->max_char_or_byte2 - font_info->min_char_or_byte2 + 1)) + (col - font_info->min_char_or_byte2)];

            if (info->character_width != 0 || (info->right_side_bearing | info->left_side_bearing | info->ascent | info->descent) != 0) {
                width += info->character_width;
            }
        }
    }

    return width;
}

static int xcb_query_text_width(const xcb_char2b_t* text, size_t textLen)
{
    /* Make the user know we’re using the slow path, but only once. */
    static bool first_invocation = true;
    if (first_invocation) {
        fprintf(stderr, "Using slow code path for text extents");
        first_invocation = false;
    }

    /* Query the text width */
    xcb_generic_error_t *error;
    xcb_query_text_extents_cookie_t cookie = xcb_query_text_extents(gConn, gsSavedFont->specific.xcb.id, textLen, (xcb_char2b_t *)text);
    xcb_query_text_extents_reply_t *reply = xcb_query_text_extents_reply(gConn, cookie, &error);
    if (reply == NULL) {
        /* We return a safe estimate because a rendering error is better than
         * a crash. Plus, the user will see the error in their log. */
        fprintf(stderr, "Could not get text extents (X error code %d)",
                error->error_code);
        free(error);
        return gsSavedFont->specific.xcb.info->max_bounds.character_width * textLen;
    }

    int width = reply->overall_width;
    free(reply);

    return width;
}

static void draw_text_pango(const char *text, size_t textLen, xcb_drawable_t drawable, cairo_surface_t *surface, int x, int y, int maxWidth, bool pangoMarkup)
{
    /* Create the Pango layout */
    /* root_visual_type is cached in load_pango_font */
    cairo_t *cr = cairo_create(surface);
    PangoLayout *layout = create_layout_with_dpi(cr);
    gint height;

    pango_layout_set_font_description(layout, gsSavedFont->specific.pangoDesc);
    pango_layout_set_width(layout, maxWidth * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    if (pangoMarkup) {
        pango_layout_set_markup(layout, text, textLen);
    }
    else {
        pango_layout_set_text(layout, text, textLen);
    }

    /* Do the drawing */
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, gsPangoFontRed, gsPangoFontGreen, gsPangoFontBlue, gsPangoFontAlpha);
    pango_cairo_update_layout(cr, layout);
    pango_layout_get_pixel_size(layout, NULL, &height);
    /* Center the piece of text vertically. */
    int yoffset = (height - gsSavedFont->height) / 2;
    cairo_move_to(cr, x, y - yoffset);
    pango_cairo_show_layout(cr, layout);

    /* Free resources */
    g_object_unref(layout);
    cairo_destroy(cr);
}

static PangoLayout* create_layout_with_dpi(cairo_t *cr)
{
    PangoLayout *layout;
    PangoContext *context;

    context = pango_cairo_create_context(cr);
    pango_cairo_context_set_resolution(context, dpi_get_value());
    layout = pango_layout_new(context);
    g_object_unref(context);

    return layout;
}

static void draw_util_set_source_color(GWMSurface* surface, GWMColor color)
{
    if (!surface_initialized(surface)) {
        return;
    }

    cairo_set_source_rgba(surface->cairo, color.red, color.green, color.blue, color.alpha);
}

