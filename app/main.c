//
// Created by dingjing on 23-11-23.
//
#include <ev.h>
#include <glib.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <glib/gi18n.h>

#include <xcb/xcb.h>                        // xcb_xkb_id
#include <xcb/xkb.h>
#include <xcb/shape.h>                      // xcb_shape_id
#include <xcb/bigreq.h>                     // xcb_big_requests_id
#include <xcb/xcb_aux.h>
#include <xcb/xcb_atom.h>
#include <xcb/xinerama.h>
#include <xcb/xcb_keysyms.h>

#include <libsn/sn-common.h>

#include "x.h"
#include "dpi.h"
#include "log.h"
#include "xcb.h"
#include "tree.h"
#include "types.h"
#include "randr.h"
#include "utils.h"
#include "cursor.h"
#include "output.h"
#include "manage.h"
#include "xinerama.h"
#include "handlers.h"
#include "container.h"
#include "scratchpad.h"
#include "command-line.h"
#include "key-bindings.h"
#include "restore-layout.h"
#include "extend-wm-hints.h"
#include "xmacro-atoms_reset.h"
#include "xmacro-atoms_NET-SUPPORTED.h"


static void main_exit                   (void);
static void main_setup_term_handlers    (void);
static void main_xcb_got_event          (EV_P_ struct ev_io *w, int rEvents);
static void main_handle_term_signal     (struct ev_loop *loop, ev_signal *signal, int rEvents);


bool                                    gXKBSupported = false;
bool                                    gShapeSupported = false;

int                                     gListenFds;
int                                     gXKBBase = 0;
int                                     gShapeBase = 0;
int                                     gRandrBase = -1;
int                                     gXKBCurrentGroup;
int                                     gXCBNubLockMask = 0;
unsigned int                            gXCBNumLockMask = 0;
char*                                   gCurConfigPath = NULL;
const char*                             gCurrentBindingMode = NULL;

xcb_key_symbols_t*                      gKeySymbols;

xcb_atom_t                              gWMSn;
xcb_atom_t                              gExtendWMHintsWindow;
xcb_window_t                            gWMSnSelectionOwner;

xcb_colormap_t                          gColormap;
uint8_t                                 gRootDepth = 0;
xcb_visualtype_t*                       gVisualType = NULL;

xcb_window_t                            gRoot = 0;
int                                     gConnScreen = 0;
xcb_timestamp_t                         gLastTimestamp = XCB_CURRENT_TIME;

xcb_connection_t*                       gConn = NULL;
struct ev_loop*                         gMainLoop = NULL;
SnDisplay*                              gSnDisplay = NULL;
xcb_screen_t*                           gRootScreen = NULL;

const char*                             gLogPath = "/tmp/graceful-wm.log";
struct ev_prepare*                      gXcbPrepare = NULL;
GWMContainer*                           gContainerRoot = NULL;
GWMContainer*                           gFocused = NULL;

GWMAllContainerHead                     gAllContainer = TAILQ_HEAD_INITIALIZER(gAllContainer);
GWMWorkspaceAssignmentsHead             gWorkspaceAssignments = TAILQ_HEAD_INITIALIZER(gWorkspaceAssignments);

GSList*                                 gConfigModes;                           // GWMConfigMode
TAILQ_HEAD(bindingsHead, Binding)*      gBindings;
SLIST_HEAD(colorPixelHead, ColorPixel)  gColorPixels;

// 定义全局 atoms
#define GWM_ATOM_MACRO(atom) xcb_atom_t A_##atom;
GWM_NET_SUPPORTED_ATOMS_XMACRO
GWM_REST_ATOMS_XMACRO
#undef GWM_ATOM_MACRO

int main(int argc, char* argv[])
{
    setlocale (LC_ALL, "");

    g_log_set_writer_func (log_handler, NULL, NULL);

    if (!command_line_parse (argc, argv)) {
        command_line_help();
        exit (-1);
    }

    // 处理命令行传入参数
    //  检测配置文件是否合法
    if (command_line_get_is_only_check_config()) {
        // FIXME:// 检查配置文件是否合法
        // return ///////
        exit (0);
    }

    // ipc socket

    // 创建与X服务的连接
    gConn = xcb_connect (NULL, &gConnScreen);
    if (xcb_connection_has_error (gConn)) {
        ERROR(_("Cannot open display"))
        exit (-1);
    }

    // 创建与显示器的连接
    gSnDisplay = sn_xcb_display_new (gConn, NULL, NULL);
    if (!gSnDisplay) {
        ERROR(_("sn_xcb_display_new error"))
        exit (-1);
    }

    // 主事件循环
    gMainLoop = EV_DEFAULT;
    if (!gMainLoop) {
        ERROR(_("ev_default_loop error"))
        exit (-1);
    }

    gRootScreen = xcb_aux_get_screen(gConn, gConnScreen);
    if (!gRootScreen) {
        ERROR(_("xcb_aux_get_screen error"));
        exit (-1);
    }
    gRoot = gRootScreen->root;

    // 获取 x11 扩展
    xcb_prefetch_extension_data (gConn, &xcb_xkb_id);
    xcb_prefetch_extension_data (gConn, &xcb_shape_id);
    xcb_prefetch_extension_data (gConn, &xcb_xinerama_id);              // FIXME://强制支持多显示器
    xcb_prefetch_extension_data (gConn, &xcb_big_requests_id);

    // 准备获取当前时间戳
    xcb_change_window_attributes (gConn, gRoot, XCB_CW_EVENT_MASK, (uint32_t[]){XCB_EVENT_MASK_PROPERTY_CHANGE});       //
    xcb_change_property (gConn, XCB_PROP_MODE_APPEND, gRoot, XCB_ATOM_SUPERSCRIPT_X, XCB_ATOM_CARDINAL, 32, 0, "");

#define GWM_ATOM_MACRO(atom) \
    xcb_intern_atom_cookie_t atom##Cookie = xcb_intern_atom(gConn, 0, strlen(#atom), #atom);
    GWM_NET_SUPPORTED_ATOMS_XMACRO
    GWM_REST_ATOMS_XMACRO
#undef GWM_ATOM_MACRO

    gRootDepth = gRootScreen->root_depth;
    gColormap = gRootScreen->default_colormap;
    gVisualType = xcb_aux_find_visual_by_attrs (gRootScreen, -1, 32);
    if (gVisualType) {
        gRootDepth = xcb_aux_get_depth_of_visual (gRootScreen, gVisualType->visual_id);
        gColormap = xcb_generate_id (gConn);
        xcb_void_cookie_t cmCookie = xcb_create_colormap_checked (gConn, XCB_COLORMAP_ALLOC_NONE, gColormap, gRoot, gVisualType->visual_id);
        xcb_generic_error_t* error = xcb_request_check (gConn, cmCookie);
        if (error) {
            ERROR(_("Could not create colormap. Error code: %d"), error->error_code)
            exit (-1);
        }
    }
    else {
        gVisualType = util_get_visual_type (gRootScreen);
    }

    xcb_prefetch_maximum_request_length (gConn);

    dpi_init();

    DEBUG(_("\nroot_depth = %d, visual_id = 0x%08x.\n"
            "root_screen->height_in_pixels = %d, root_screen->height_in_millimeters = %d\n"
            "One logical pixel corresponds to %d physical pixel on this display.\n"),
            gRootDepth, gVisualType->visual_id,
            gRootScreen->height_in_pixels, gRootScreen->height_in_millimeters,
            dpi_logical_px(1))

    xcb_get_geometry_cookie_t geoCookie = xcb_get_geometry (gConn, gRoot);
    xcb_query_pointer_cookie_t geoPointerCookie = xcb_query_pointer (gConn, gRoot);

    xcb_flush (gConn);
    {
        xcb_generic_event_t* ev;
        DEBUG(_("Waiting for PropertyNotify event"));
        while (NULL != (ev = xcb_wait_for_event (gConn))) {
            if (XCB_PROPERTY_NOTIFY == ev->response_type) {
                gLastTimestamp = ((xcb_property_notify_event_t*) ev)->time;
                free (ev);
                break;
            }
            free (ev);
        }
        DEBUG(_("Got timestamp %d"), gLastTimestamp);
    }

    // Setup NetWM atoms
#define GWM_ATOM_MACRO(name)                                                                \
    do {                                                                                    \
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(gConn, name##Cookie, NULL);  \
        if (!reply) {                                                                       \
            ERROR("Could not get atom " #name);                                             \
            exit(-1);                                                                       \
        }                                                                                   \
        A_##name = reply->atom;                                                             \
        free(reply);                                                                        \
    } while (0);
    GWM_NET_SUPPORTED_ATOMS_XMACRO
    GWM_REST_ATOMS_XMACRO
#undef GWM_ATOM_MACRO

    // 加载配置文件

    // UNIX domain socket

    // WM_Sn selection
    {
        char* atomName = xcb_atom_name_by_screen ("WM", gConnScreen);
        gWMSnSelectionOwner = xcb_generate_id (gConn);
        if (NULL == atomName) {
            ERROR(_("xcb_atom_name_by_screen(\"WM\", %d) failed, exiting"), gConnScreen);
            exit (-1);
        }

        xcb_intern_atom_reply_t* atomReply = xcb_intern_atom_reply (gConn, xcb_intern_atom_unchecked (gConn, 0, strlen (atomName), atomName), NULL);
        free (atomName);
        if (NULL == atomReply) {
            ERROR(_("Failed to intern the WM_Sn atom, exiting"));
            exit (-1);
        }

        gWMSn = atomReply->atom;
        free (atomReply);

        // check if the selection is already owned
        xcb_get_selection_owner_reply_t* selectionReply = xcb_get_selection_owner_reply (gConn, xcb_get_selection_owner (gConn, gWMSn), NULL);
        if (selectionReply && XCB_NONE != selectionReply->owner && !command_line_get_is_replace()) {
            ERROR(_("Another window manager is already running (WM_Sn is owned)"));
            printf (_("Another window manager is already running (WM_Sn is owned)"));
            exit (0);
        }

        // Become the selection owner
        xcb_create_window (gConn, gRootScreen->root_depth, gWMSnSelectionOwner, gRootScreen->root, -1, -1, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, gRootScreen->root_visual, 0, NULL);
        xcb_change_property (gConn, XCB_PROP_MODE_REPLACE, gWMSnSelectionOwner, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, (strlen ("graceful-wm") + 1) * 2, "graceful-wm\0graceful-wm\0");
        xcb_set_selection_owner (gConn, gWMSnSelectionOwner, gWMSn, gLastTimestamp);
        if (selectionReply && XCB_NONE != selectionReply->owner) {
            unsigned int uSleepTime = 100000;
            int checkRounds = 150;
            xcb_get_geometry_reply_t* geomReply = NULL;
            DEBUG(_("Waiting for old WM_Sn selection owner to exit"));
            do {
                free (geomReply);
                usleep (uSleepTime);
                if (0 == --checkRounds) {
                    ERROR(_("The old window manager is not exiting"));
                    return -1;
                }
                geomReply = xcb_get_geometry_reply (gConn, xcb_get_geometry (gConn, selectionReply->owner), NULL);
            } while (NULL != geomReply);
        }
        free (selectionReply);

        union {
            xcb_client_message_event_t message;
            char storage[32];
        } event;
        memset (&event, 0, sizeof (event));
        event.message.response_type = XCB_CLIENT_MESSAGE;
        event.message.window = gRootScreen->root;
        event.message.format = 32;
        event.message.type = A_MANAGER;
        event.message.data.data32[0] = gLastTimestamp;
        event.message.data.data32[1] = gWMSn;
        event.message.data.data32[2] = gWMSnSelectionOwner;

        xcb_send_event (gConn, 0, gRootScreen->root, XCB_EVENT_MASK_STRUCTURE_NOTIFY, event.storage);
    }

    xcb_void_cookie_t cookie = xcb_change_window_attributes_checked (gConn, gRoot, XCB_CW_EVENT_MASK, (uint32_t[]){ ROOT_EVENT_MASK });
    xcb_generic_error_t* error = xcb_request_check (gConn, cookie);
    if (NULL != error) {
        ERROR(_("Another window manager seems to be running (X error %d)"), error->error_code);
        return 1;
    }

    xcb_get_geometry_reply_t* geoReply = xcb_get_geometry_reply (gConn, geoCookie, NULL);
    if (NULL == geoReply) {
        ERROR(_("Could not get geometry of the root window, exiting."));
        return 1;
    }

    DEBUG(_("Root geometry reply: (%d %d) %d x %d"), geoReply->x, geoReply->y, geoReply->width, geoReply->height);

    // load cursor
    cursor_load_cursor();

    // 根窗口设置鼠标
    cursor_set_root_cursor (CURSOR_POINTER);

    const xcb_query_extension_reply_t* extReply = xcb_get_extension_data (gConn, &xcb_xkb_id);
    gXKBSupported = extReply->present;
    if (!extReply->present) {
        DEBUG(_("xkb is not present on this server."));
    }
    else {
        DEBUG(_("initializing xcb-xkb"));
        xcb_xkb_use_extension (gConn, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
        xcb_xkb_select_events (gConn, XCB_XKB_ID_USE_CORE_KBD,
                                XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY,
                                0,
                                XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY,
                                0xFF, 0xFF, NULL);
        const uint32_t mask = XCB_XKB_PER_CLIENT_FLAG_GRABS_USE_XKB_STATE | XCB_XKB_PER_CLIENT_FLAG_LOOKUP_STATE_WHEN_GRABBED | XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT;
        xcb_xkb_per_client_flags_reply_t* pcfReply = xcb_xkb_per_client_flags_reply (gConn, xcb_xkb_per_client_flags (gConn, XCB_XKB_ID_USE_CORE_KBD, mask, mask, 0, 0, 0), NULL);

#define PCF_REPLY_ERROR(_val)                                       \
        do {                                                        \
            if (NULL == pcfReply || !(pcfReply->value & (_val))) {  \
                ERROR(_("Could not set #_val"));                    \
            }                                                       \
        } while(0)

        PCF_REPLY_ERROR(XCB_XKB_PER_CLIENT_FLAG_GRABS_USE_XKB_STATE);
        PCF_REPLY_ERROR(XCB_XKB_PER_CLIENT_FLAG_LOOKUP_STATE_WHEN_GRABBED);
        PCF_REPLY_ERROR(XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT);

        free (pcfReply);
        gXKBBase = extReply->first_event;
    }

    // Check for shape extension.
    extReply = xcb_get_extension_data (gConn, &xcb_shape_id);
    if (extReply->present) {
        gShapeBase = extReply->first_event;
        xcb_shape_query_version_cookie_t cookie = xcb_shape_query_version (gConn);
        xcb_shape_query_version_reply_t* version = xcb_shape_query_version_reply (gConn, cookie, NULL);
        gShapeSupported = (version && version->minor_version >= 1);
        free (version);
    }
    else {
        gShapeSupported = false;
    }

    if (!gShapeSupported) {
        DEBUG(_("Shape 1.1 is not present on this server"));
    }

    // restore connect
//    restore_connect();

    handler_property_init();

    extend_wm_hint_setup_hint();

    gKeySymbols = xcb_key_symbols_alloc (gConn);

    gXCBNubLockMask = (int) util_aio_get_mod_mask_for ((uint32_t) XCB_NUM_LOCK, gKeySymbols);

    if (!key_binding_load_keymap()) {
        ERROR(_("Could not load keymap"));
        return -1;
    }

    key_binding_translate_keysyms();
    key_binding_grab_all_keys (gConn);

    DEBUG("start tree init");
    tree_init (geoReply);
    free (geoReply);
    DEBUG("end tree init!");

//    xinerama_init();
//
    scratchpad_fix_resolution();

    xcb_query_pointer_reply_t* pointerReply = NULL;

    // output
    GWMOutput* output = NULL;
    if (!(pointerReply = xcb_query_pointer_reply (gConn, geoPointerCookie, NULL))) {
        ERROR(_("Could not query pointer position, using first screen"));
    }
    else {
        DEBUG(_("Pointer at %d, %d"), pointerReply->root_x, pointerReply->root_y);
        output = randr_get_output_containing (pointerReply->root_x, pointerReply->root_y);
        if (!output) {
            ERROR(_("ERROR: No screen at (%d, %d), starting on the first screen"), pointerReply->root_x, pointerReply->root_y);
        }
    }

    if (!output) {
        output = randr_get_first_output();
    }

//    container_activate(container_descend_focused (output_get_content (output->container)));
    free (pointerReply);

    tree_render();

    x_set_gwm_atoms();

    extend_wm_hint_update_work_area();

    // 设置桌面属性
    extend_wm_hint_update_desktop_properties();

    struct ev_io* xcbWatcher = g_malloc0 (sizeof (struct ev_io));
    g_assert(xcbWatcher);

    gXcbPrepare = g_malloc0 (sizeof (struct ev_prepare));
    g_assert(gXcbPrepare);

    ev_io_init(xcbWatcher, main_xcb_got_event, xcb_get_file_descriptor (gConn), EV_READ);
    ev_io_start (gMainLoop, xcbWatcher);

    xcb_flush (gConn);

    // 捕获服务器，丢弃旧的事件
    xcb_grab_server (gConn);
    {
        xcb_aux_sync (gConn);
        xcb_generic_event_t* event;
        while (NULL != (event = xcb_poll_for_event(gConn))) {
            if (0 == event->response_type) {
                free (event);
                DEBUG("...");
                continue;
            }

            int type = (event->response_type & 0x7F);       // 去掉最高位
            if (XCB_MAP_REQUEST == type) {
                handler_handle_event (type, event);
            }
            free (event);
        }
        manage_existing_windows (gRoot);
    }
    xcb_ungrab_server (gConn);

    {
        // sigaction
    }

    main_setup_term_handlers ();

    signal (SIGPIPE, SIG_IGN);

    // start bar

    atexit (main_exit);

    // 开始运行 graceful window manager
    ev_loop (gMainLoop, 0);

    // free
    {
        free (xcbWatcher);
        xcbWatcher = NULL;
    }

    return 0;
}

static void main_exit (void)
{

}

static void main_setup_term_handlers (void)
{
    static struct ev_signal signal_watchers[6];
    size_t num_watchers = sizeof(signal_watchers) / sizeof(signal_watchers[0]);

    ev_signal_init(&signal_watchers[0], main_handle_term_signal, SIGHUP);
    ev_signal_init(&signal_watchers[1], main_handle_term_signal, SIGINT);
    ev_signal_init(&signal_watchers[2], main_handle_term_signal, SIGALRM);
    ev_signal_init(&signal_watchers[3], main_handle_term_signal, SIGTERM);
    ev_signal_init(&signal_watchers[4], main_handle_term_signal, SIGUSR1);
    ev_signal_init(&signal_watchers[5], main_handle_term_signal, SIGUSR1);

    for (size_t i = 0; i < num_watchers; i++) {
        ev_signal_start(gMainLoop, &signal_watchers[i]);
        ev_unref(gMainLoop);
    }
}

static void main_xcb_got_event(EV_P_ struct ev_io *w, int rEvents)
{
    /* empty, because xcb_prepare_cb are used */
}
static void main_handle_term_signal(struct ev_loop *loop, ev_signal *signal, int rEvents)
{
    exit(128 + signal->signum);
}