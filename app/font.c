//
// Created by dingjing on 23-12-10.
//

#include "font.h"

#include <pango/pangocairo.h>
#include <cairo-xcb.h>

#include "val.h"
#include "log.h"
#include "dpi.h"


static PangoLayout *create_layout_with_dpi(cairo_t *cr);
static bool load_pango_font(GWMFont *font, const char *desc);
static int xcb_query_text_width(const xcb_char2b_t *text, size_t text_len);
static int predict_text_width_xcb(const xcb_char2b_t *text, size_t text_len);
static int predict_text_width_pango(const char *text, size_t textLen, bool pangoMarkup);
static void draw_text_xcb(const xcb_char2b_t *text, size_t text_len, xcb_drawable_t drawable, xcb_gcontext_t gc, int x, int y);
static void draw_text_pango(const char *text, size_t textLen, xcb_drawable_t drawable, cairo_surface_t *surface, int x, int y, int maxWidth, bool pangoMarkup);

static const GWMFont*       gsSavedFont = NULL;
static double               gsPangoFontRed;
static double               gsPangoFontGreen;
static double               gsPangoFontBlue;
static double               gsPangoFontAlpha;


bool font_is_pango(void)
{
    return gsSavedFont->type == FONT_TYPE_PANGO;
}

GWMFont font_load_font(const char *pattern, bool fallback)
{
    font_free_font();

    GWMFont font;
    font.type = FONT_TYPE_NONE;
    font.pattern = NULL;

    if (gConn == NULL) {
        return font;
    }

    if (strlen(pattern) > strlen("pango:") && !strncmp(pattern, "pango:", strlen("pango:"))) {
        const char *font_pattern = pattern + strlen("pango:");
        if (load_pango_font(&font, font_pattern)) {
            font.pattern = g_strdup(pattern);
            return font;
        }
    } else if (strlen(pattern) > strlen("xft:") && !strncmp(pattern, "xft:", strlen("xft:"))) {
        const char *font_pattern = pattern + strlen("xft:");
        if (load_pango_font(&font, font_pattern)) {
            font.pattern = g_strdup(pattern);
            return font;
        }
    }

    font.specific.xcb.id = xcb_generate_id(gConn);
    xcb_void_cookie_t font_cookie = xcb_open_font_checked(gConn, font.specific.xcb.id, strlen(pattern), pattern);
    xcb_query_font_cookie_t info_cookie = xcb_query_font(gConn, font.specific.xcb.id);

    xcb_generic_error_t *error;
    error = xcb_request_check(gConn, font_cookie);

    if (fallback && error != NULL) {
        ERROR("Could not open font %s (X error %d). Trying fallback to 'fixed'.\n", pattern, error->error_code);
        pattern = "fixed";
        font_cookie = xcb_open_font_checked(gConn, font.specific.xcb.id, strlen(pattern), pattern);
        info_cookie = xcb_query_font(gConn, font.specific.xcb.id);

        free(error);
        error = xcb_request_check(gConn, font_cookie);

        if (error != NULL) {
            ERROR("Could not open fallback font 'fixed', trying with '-misc-*'.\n");
            pattern = "-misc-*";
            font_cookie = xcb_open_font_checked(gConn, font.specific.xcb.id, strlen(pattern), pattern);
            info_cookie = xcb_query_font(gConn, font.specific.xcb.id);

            free(error);
            if ((error = xcb_request_check(gConn, font_cookie)) != NULL) {
                DIE("Could open neither requested font nor fallbacks (fixed or -misc-*): X11 error %d", error->error_code);
            }
        }
    }
    free(error);

    font.pattern = g_strdup(pattern);
    DEBUG("Using X font %s\n", pattern);

    if (!(font.specific.xcb.info = xcb_query_font_reply(gConn, info_cookie, NULL))) {
        DIE(EXIT_FAILURE, "Could not load font \"%s\"", pattern);
    }

    if (xcb_query_font_char_infos_length(font.specific.xcb.info) == 0) {
        font.specific.xcb.table = NULL;
    }
    else {
        font.specific.xcb.table = xcb_query_font_char_infos(font.specific.xcb.info);
    }

    font.height = font.specific.xcb.info->font_ascent + font.specific.xcb.info->font_descent;

    font.type = FONT_TYPE_XCB;

    return font;
}

void font_set_font(GWMFont *font)
{
    gsSavedFont = font;
}

int font_predict_text_width(const char* text)
{
    g_assert(gsSavedFont != NULL);

    switch (gsSavedFont->type) {
        case FONT_TYPE_NONE:
            /* Nothing to do */
            return 0;
        case FONT_TYPE_XCB:
            return predict_text_width_xcb((text), strlen(text));
        case FONT_TYPE_PANGO:
            /* Calculate extents using Pango */
            return predict_text_width_pango((text), strlen(text), false);
    }
    g_assert(false);
}

void font_free_font(void)
{
    if (gsSavedFont == NULL) {
        return;
    }

    free(gsSavedFont->pattern);
    switch (gsSavedFont->type) {
        case FONT_TYPE_NONE: {
            break;
        }
        case FONT_TYPE_XCB: {
            xcb_close_font(gConn, gsSavedFont->specific.xcb.id);
            free(gsSavedFont->specific.xcb.info);
            break;
        }
        case FONT_TYPE_PANGO: {
            pango_font_description_free(gsSavedFont->specific.pangoDesc);
            break;
        }
    }
    gsSavedFont = NULL;
}

void font_draw_text(const char*text, xcb_drawable_t drawable, xcb_gcontext_t gc, cairo_surface_t *surface, int x, int y, int maxWidth)
{
    g_assert(gsSavedFont != NULL);

    switch (gsSavedFont->type) {
        case FONT_TYPE_NONE: {
            return;
        }
        case FONT_TYPE_XCB: {
            draw_text_xcb((text), strlen(text), drawable, gc, x, y);
            break;
        }
        case FONT_TYPE_PANGO: {
            draw_text_pango((text), strlen(text), drawable, surface, x, y, maxWidth, false);
            return;
        }
    }
}

void font_set_font_colors(xcb_gcontext_t gc, GWMColor foreground, GWMColor background)
{
    g_assert(gsSavedFont != NULL);

    switch (gsSavedFont->type) {
        case FONT_TYPE_NONE: {
            break;
        }
        case FONT_TYPE_XCB: {
            uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
            uint32_t values[] = {foreground.colorPixel, background.colorPixel, gsSavedFont->specific.xcb.id};
            xcb_change_gc(gConn, gc, mask, values);
            break;
        }
        case FONT_TYPE_PANGO: {
            gsPangoFontRed = foreground.red;
            gsPangoFontGreen = foreground.green;
            gsPangoFontBlue = foreground.blue;
            gsPangoFontAlpha = foreground.alpha;
            break;
        }
    }
}

static PangoLayout *create_layout_with_dpi(cairo_t *cr)
{
    PangoLayout *layout;
    PangoContext *context;

    context = pango_cairo_create_context(cr);
    pango_cairo_context_set_resolution(context, dpi_get_value());
    layout = pango_layout_new(context);
    g_object_unref(context);

    return layout;
}


static bool load_pango_font(GWMFont *font, const char *desc)
{
    font->specific.pangoDesc = pango_font_description_from_string(desc);
    if (!font->specific.pangoDesc) {
        ERROR("Could not open font %s with Pango, fallback to X font.\n", desc);
        return false;
    }

    DEBUG("Using Pango font %s, size %d\n",
        pango_font_description_get_family(font->specific.pangoDesc),
        pango_font_description_get_size(font->specific.pangoDesc) / PANGO_SCALE);

    cairo_surface_t *surface = cairo_xcb_surface_create(gConn, gRootScreen->root, gVisualType, 1, 1);
    cairo_t *cr = cairo_create(surface);
    PangoLayout *layout = create_layout_with_dpi(cr);
    pango_layout_set_font_description(layout, font->specific.pangoDesc);

    gint height;
    pango_layout_get_pixel_size(layout, NULL, &height);
    font->height = height;

    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    font->type = FONT_TYPE_PANGO;
    return true;
}

static void draw_text_pango(const char *text, size_t textLen, xcb_drawable_t drawable, cairo_surface_t *surface, int x, int y, int maxWidth, bool pangoMarkup)
{
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

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, gsPangoFontRed, gsPangoFontGreen, gsPangoFontBlue, gsPangoFontAlpha);
    pango_cairo_update_layout(cr, layout);
    pango_layout_get_pixel_size(layout, NULL, &height);
    int yOffset = (height - gsSavedFont->height) / 2;
    cairo_move_to(cr, x, y - yOffset);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
    cairo_destroy(cr);
}

static int predict_text_width_pango(const char *text, size_t textLen, bool pangoMarkup)
{
    cairo_surface_t *surface = cairo_xcb_surface_create(gConn, gRootScreen->root, gVisualType, 1, 1);
    cairo_t *cr = cairo_create(surface);
    PangoLayout *layout = create_layout_with_dpi(cr);

    gint width;
    pango_layout_set_font_description(layout, gsSavedFont->specific.pangoDesc);

    if (pangoMarkup) {
        pango_layout_set_markup(layout, text, textLen);
    }
    else {
        pango_layout_set_text(layout, text, textLen);
    }

    pango_cairo_update_layout(cr, layout);
    pango_layout_get_pixel_size(layout, &width, NULL);

    /* Free resources */
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return width;
}

static void draw_text_xcb(const xcb_char2b_t *text, size_t text_len, xcb_drawable_t drawable, xcb_gcontext_t gc, int x, int y)
{
    int pos_y = y + gsSavedFont->specific.xcb.info->font_ascent;

    int offset = 0;
    for (;;) {
        int chunk_size = (text_len > 255 ? 255 : text_len);
        const xcb_char2b_t *chunk = text + offset;

        xcb_image_text_16(gConn, chunk_size, drawable, gc, x, pos_y, chunk);

        offset += chunk_size;
        text_len -= chunk_size;

        if (text_len == 0) {
            break;
        }
        x += predict_text_width_xcb(chunk, chunk_size);
    }
}

static int xcb_query_text_width(const xcb_char2b_t *text, size_t text_len)
{
    /* Make the user know we’re using the slow path, but only once. */
    static bool first_invocation = true;
    if (first_invocation) {
        fprintf(stderr, "Using slow code path for text extents\n");
        first_invocation = false;
    }

    /* Query the text width */
    xcb_generic_error_t *error;
    xcb_query_text_extents_cookie_t cookie = xcb_query_text_extents(gConn, gsSavedFont->specific.xcb.id, text_len, (xcb_char2b_t *)text);
    xcb_query_text_extents_reply_t *reply = xcb_query_text_extents_reply(gConn, cookie, &error);
    if (reply == NULL) {
        fprintf(stderr, "Could not get text extents (X error code %d)\n", error->error_code);
        free(error);
        return gsSavedFont->specific.xcb.info->max_bounds.character_width * text_len;
    }

    int width = reply->overall_width;
    free(reply);
    return width;
}

static int predict_text_width_xcb(const xcb_char2b_t *input, size_t text_len)
{
    if (text_len == 0)
        return 0;

    int width;
    if (gsSavedFont->specific.xcb.table == NULL) {
        width = xcb_query_text_width(input, text_len);
    }
    else {
        xcb_query_font_reply_t *font_info = gsSavedFont->specific.xcb.info;
        xcb_charinfo_t *font_table = gsSavedFont->specific.xcb.table;

        width = 0;
        for (size_t i = 0; i < text_len; i++) {
            xcb_charinfo_t *info;
            int row = input[i].byte1;
            int col = input[i].byte2;
            if (row < font_info->min_byte1
                || row > font_info->max_byte1
                || col < font_info->min_char_or_byte2
                || col > font_info->max_char_or_byte2) {
                continue;
            }

            info = &font_table[((row - font_info->min_byte1) * (font_info->max_char_or_byte2 - font_info->min_char_or_byte2 + 1))
                                + (col - font_info->min_char_or_byte2)];

            if (info->character_width != 0 || (info->right_side_bearing | info->left_side_bearing | info->ascent | info->descent) != 0) {
                width += info->character_width;
            }
        }
    }

    return width;
}
