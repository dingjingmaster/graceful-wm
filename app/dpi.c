//
// Created by dingjing on 23-11-27.
//

#include "dpi.h"


#include <math.h>
#include <stdlib.h>

#include <xcb/xcb_xrm.h>

#include "val.h"
#include "log.h"

static long dpi;



static long init_dpi_fallback(void)
{
    return (long) ((double)gRootScreen->height_in_pixels * 25.4 / (double)gRootScreen->height_in_millimeters);
}

void dpi_init(void)
{
    xcb_xrm_database_t *database = NULL;
    char *resource = NULL;

    if (NULL == gConn) {
        goto init_dpi_end;
    }

    database = xcb_xrm_database_from_default(gConn);
    if (NULL == database) {
        ERROR("Failed to open the resource database.");
        goto init_dpi_end;
    }

    xcb_xrm_resource_get_string(database, "Xft.dpi", NULL, &resource);
    if (resource == NULL) {
        DEBUG("Resource Xft.dpi not specified, skipping.");
        goto init_dpi_end;
    }

    char *endPtr;
    double inDPI = strtod(resource, &endPtr);
    if (inDPI == HUGE_VAL || dpi < 0 || *endPtr != '\0' || endPtr == resource) {
        ERROR("Xft.dpi = %s is an invalid number and couldn't be parsed.", resource);
        dpi = 0;
        goto init_dpi_end;
    }
    dpi = lround(inDPI);

    DEBUG("Found Xft.dpi = %ld.", dpi);

init_dpi_end:
    free(resource);

    if (database != NULL) {
        xcb_xrm_database_free(database);
    }

    if (dpi == 0) {
        DEBUG("Using fallback for calculating DPI.");
        dpi = init_dpi_fallback();
        DEBUG("Using dpi = %ld", dpi);
    }
}

long dpi_get_value(void)
{
    return dpi;
}

int dpi_logical_px(int logical)
{
    if (NULL == gRootScreen) {
        return logical;
    }

    if (((double) dpi / 96.0) < 1.25) {
        return logical;
    }

    return ceil(((double) dpi / 96.0) * logical);
}