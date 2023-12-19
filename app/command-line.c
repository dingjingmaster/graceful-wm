//
// Created by dingjing on 23-11-23.
//

#include "command-line.h"

#include <glib.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib/gi18n.h>

static int              isReplace = 0;
static char*            configFile = NULL;
static int              isShowVersion = 0;
static int              isOnlyCheckConf = 0;

static GOptionContext* gOptionCtx = NULL;
static GOptionEntry gEntries[] =
    {
        {"replace", 'r', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_INT, &isReplace, N_("Replace an existing window manager.")},
        {"config", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME, &configFile, N_("Custom profile path (default :/etc/graceful-wm.ini).")},
        {"check", 'C', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_FILENAME, &isOnlyCheckConf, N_("Exits after detecting the configuration file.")},
        {"version", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_INT, &isShowVersion, N_("Display version information.")},
        NULL
    };


bool command_line_parse(const int argc, int **argv)
{
    g_return_val_if_fail(argc && argv, false);

    g_autoptr(GError) error = NULL;
    gOptionCtx = g_option_context_new (NULL);
    g_option_context_set_help_enabled (gOptionCtx, true);
    g_option_context_set_ignore_unknown_options (gOptionCtx, true);
    g_option_context_add_main_entries (gOptionCtx, gEntries, NULL);
    g_option_context_set_description (gOptionCtx, N_("This is a lightweight window manager."));

    if (!g_option_context_parse (gOptionCtx, &argc, &argv, &error)) {
        printf (_("g_option_context_parse error: %s"), (error ? error->message : _("Unknown")));
        return false;
    }

    return true;
}

void command_line_help()
{
    printf ("%s\n", g_option_context_get_help (gOptionCtx, true, NULL));
}

bool command_line_get_is_replace()
{
    return (isReplace != 0);
}

bool command_line_get_is_only_check_config()
{
    return (1 == isOnlyCheckConf);
}

const char *command_line_get_config_path()
{
    return NULL;
}

GWMConfigLoad command_line_get_load_type()
{
    return C_RELOAD;
}
