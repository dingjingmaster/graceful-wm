//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_TYPES_H
#define GRACEFUL_WM_TYPES_H
#include <pcre2.h>
#include <stdint.h>
#include <stdbool.h>
#include <xcb/xcb.h>
#include <xcb/shape.h>
#include <xcb/randr.h>
#include <cairo/cairo.h>
#include <pango/pango.h>
#include <libsn/sn-launcher.h>


typedef enum Layout                     GWMLayout;                  // ok
typedef enum GapsMask                   GapsMask;                   // ok
typedef enum FullScreenMode             GWMFullScreenMode;          // ok
typedef enum BorderStyle                GWMBorder;                  // ok
typedef enum KillWindow                 GWMKillWindow;              // ok

typedef struct Output                   GWMOutput;                  // ok
typedef struct OutputName               GWMOutputName;              // ok
typedef struct Gaps                     Gaps;                       // ok
typedef struct Size                     Size;                       // ok
typedef struct Rect                     GWMRect;                    // ok
typedef struct Regex                    Regex;                      // ok
typedef struct Match                    Match;                      // ok
typedef struct Window                   GWMWindow;                  // ok
typedef struct Font                     GWMFont;                    // ok
typedef struct Color                    GWMColor;                   // ok
typedef struct Surface                  Surface;                    // ok
typedef struct Assignment               Assignment;                 // ok
typedef struct Container                GWMContainer;               // ok
typedef struct ReserveEdgePixels        ReserveEdgePixels;          // ok
typedef struct DecorationRenderParams   GWMDecorationRenderParams;  // ok


enum BorderStyle
{
    BS_NONE         = 0,
    BS_PIXEL        = 1,
    BS_NORMAL       = 2,
};

enum FullScreenMode
{
    CF_NONE         = 0,
    CF_OUTPUT       = 1,
    CF_GLOBAL       = 2,
};

enum Layout
{
    L_DEFAULT       = 0,
    L_STACKED       = 1,
    L_TABBED        = 2,
    L_DOCK_AREA     = 3,
    L_OUTPUT        = 4,
    L_SPLIT_V       = 5,
    L_SPLIT_H       = 6,
};

enum GapsMask
{
    GAPS_INNER      = (1 << 0),
    GAPS_TOP        = (1 << 1),
    GAPS_RIGHT      = (1 << 2),
    GAPS_BOTTOM     = (1 << 3),
    GAPS_LEFT       = (1 << 4),
    GAPS_VERTICAL   = (GAPS_TOP | GAPS_BOTTOM),
    GAPS_HORIZONTAL = (GAPS_RIGHT | GAPS_LEFT),
    GAPS_OUTER      = (GAPS_VERTICAL | GAPS_HORIZONTAL),
};

enum KillWindow
{
    KILL_WINDOW_DO_NOT  = 0,
    KILL_WINDOW         = 1,
    KILL_CLIENT         = 2,
};

struct Regex
{
    char*                       pattern;
    pcre2_code*                 regex;
};

struct Size
{
    uint32_t                    w;
    uint32_t                    h;
};

struct Rect
{
    uint32_t                    x;
    uint32_t                    y;
    uint32_t                    width;
    uint32_t                    height;
};

struct Gaps
{
    int                         inner;
    int                         top;
    int                         right;
    int                         bottom;
    int                         left;
};

struct Color
{
    double                      red;
    double                      green;
    double                      blue;
    double                      alpha;

    uint32_t                    colorPixel;
};

struct ReserveEdgePixels
{
    uint32_t                    left;
    uint32_t                    right;
    uint32_t                    top;
    uint32_t                    bottom;
};

struct OutputName
{
    char*                       name;
    GSList*                     names;              // OutputName
};

struct Output
{
    xcb_randr_output_t          id;
    bool                        active;
    bool                        changed;
    bool                        primary;
    bool                        toBeDisabled;

    GQueue                      namesHead;          // OutputName
    GQueue                      outputs;            // Output

    GWMContainer*               container;
    GWMRect                     rect;
};

struct Font
{
    enum {
        FONT_TYPE_NONE = 0,
        FONT_TYPE_XCB,
        FONT_TYPE_PANGO
    } type;

    int                         height;             // The height of the font, built from font_ascent + font_descent
    char*                       pattern;            // The pattern/name used to load the font.

    union {
        struct {
            xcb_font_t              id;              // The xcb-id for the font
            xcb_query_font_reply_t* info;            // Font information gathered from the server
            xcb_charinfo_t*         table;           // Font table for this font (may be NULL)
        } xcb;

        PangoFontDescription*   pangoDesc;           // The pango font description
    } specific;
};

struct DecorationRenderParams
{
    struct Colortriple*         color;
    int                         borderStyle;
    Size                        containerSize;
    Size                        containerWindowSize;
    GWMRect                     containerDecorationRect;
    GWMColor                    background;
    GWMLayout                   parentLayout;
    bool                        containerIsLeaf;
};

struct Match
{
    char*                       error;

    Regex*                      mark;
    Regex*                      title;
    Regex*                      class;
    Regex*                      machine;
    Regex*                      instance;
    Regex*                      workspace;
    Regex*                      windowRole;
    Regex*                      application;

    xcb_atom_t                  windowType;
    enum {
        U_DO_NOT_CHECK  = -1,
        U_LATEST        = 0,
        U_OLDEST        = 1
    } urgent;
    enum {
        M_DO_NOT_CHECK  = -1,
        M_NO_DOCK       = 0,
        M_DOCK_ANY      = 1,
        M_DOCK_TOP      = 2,
        M_DOCK_BOTTOM   = 3
    } dock;
    xcb_window_t                id;
    enum {
        WM_ANY          = 0,
        WM_TILING_AUTO,
        WM_TILING_USER,
        WM_TILING,
        WM_FLOATING_AUTO,
        WM_FLOATING_USER,
        WM_FLOATING
    } windowMode;
    GWMContainer*               containerID;
    bool                        matchAllWindows;

    enum {
        M_HERE          = 0,
        M_ASSIGN_WS,
        M_BELOW
    } insertWhere;

    GQueue                      matches;                // element is Match
    bool                        restartMode;
};

struct Assignment
{
    enum {
        A_ANY                   = 0,
        A_COMMAND               = (1 << 0),
        A_TO_WORKSPACE          = (1 << 1),
        A_NO_FOCUS              = (1 << 2),
        A_TO_WORKSPACE_NUMBER   = (1 << 3),
        A_TO_OUTPUT             = (1 << 4),
    };

    Match                       match;

    union {
        char*       command;
        char*       workspace;
        char*       output;
    } destination;
    GQueue                      assignments;
};

struct Window
{
    xcb_window_t                id;
    xcb_window_t                leader;                 // 保存leader窗口(工具窗口和类似浮动窗口的逻辑父窗口)ID
    xcb_window_t                transientFor;           //
    uint32_t                    nrAssignments;          // 指向已经在此窗口运行过的赋值的指针(赋值只运行一次)
    Assignment**                ranAssignments;         //

    char*                       classClass;
    char*                       classInstance;
    GString*                    name;

    char*                       role;                   // WM_WINDOW_ROLE of this window
    char*                       machine;                // WM_CLIENT_MACHINE of the window
    bool                        nameXChanged;           //
    bool                        usesNetWMName;          //
    bool                        needTakeFocus;          //
    bool                        doesNotAcceptFocus;     //
    xcb_atom_t                  windowType;
    uint32_t                    wmDesktop;
    enum {
        W_NO_DOCK       = 0,
        W_DOCK_TOP      = 1,
        W_DOCK_BOTTOM   =2,
    } dock;
    struct timeval              urgent;                 // Window何时被标记为紧急? 0表示不紧急
    ReserveEdgePixels           reserved;               //

    uint16_t                    depth;

    int                         baseWidth;
    int                         baseHeight;

    int                         widthInc;
    int                         heightInc;

    int                         minWidth;
    int                         minHeight;

    int                         maxWidth;
    int                         maxHeight;

    double                      minAspectRatio;
    double                      maxAspectRatio;

    cairo_surface_t*            icon;

    bool                        shaped;
    bool                        inputShaped;
    time_t                      managedSince;
    bool                        swallowed;

    //FIXME://
};

struct Surface
{
    bool                        ownsGC;

    int                         width;
    int                         height;

    xcb_drawable_t              id;             // The drawable which is being represented
    xcb_gcontext_t              gc;             // XCB graphics context

    cairo_t*                    cairo;
    cairo_surface_t*            surface;        // A cairo surface representing the drawable
};


struct Container
{
    bool                        mapped;
    bool                        urgent;             // 是否加急处理，如果容器内有窗口设置加急则设置
    bool                        pixmapRecreated;    //
    uint8_t                     ignoreUnmap;        // 该容器应该忽略的UnmapNotify事件的数量(如果UnmapNotify事件是由本身引起的，则需要忽略这些事件。例如，在重新定位或在工作区更改时取消对窗口的映射时。)
    Surface                     frame;
    Surface                     frameBuffer;

    enum {
        CT_ROOT = 0,
        CT_OUTPUT = 1,
        CT_CON = 2,
        CT_FLOATING_CON = 3,
        CT_WORKSPACE = 4,
        CT_DOCK_AREA = 5,
    } type;

    int                         workspaceNum;
    Gaps                        gaps;
    GWMContainer*               parent;
    GWMRect                     rect;
    GWMRect                     windowRect;
    GWMRect                     decorationRect;
    GWMRect                     geoRect;

    char*                       name;
    char*                       titleFormat;

    int                         windowIconPadding;
    char*                       stickyGroup;                // sticky group 它将多个容器捆绑到一个组中。内容在所有容器之间共享，也就是说，它们显示在当前可见的任何一个容器上

    bool                        markChanged;                // 缓存以确定是否需要重新绘制
    GQueue                      marksHead;                  // 用户可定义标记，以便稍后跳转到此容器
    double                      percent;

    int                         borderWidth;
    int                         currentBorderWidth;

    Window*                     window;                     //
    struct ev_timer*            urgencyTimer;
    GWMDecorationRenderParams*  decorationRenderParams;
    GQueue                      floatingHead;               // Container
    GQueue                      nodesHead;                  // Container
    GQueue                      focusHead;                  // Container
    GQueue                      swallowHead;                // Match
    GWMFullScreenMode           fullScreenMode;

    bool                        sticky;

    GWMLayout                   layout;
    GWMLayout                   lastSplitLayout;
    GWMLayout                   workspaceLayout;

    GWMBorder                   borderStyle;
    GWMBorder                   maxUserBorderStyle;

    enum {
        FLOATING_AUTO_OFF   = 0,
        FLOATING_USER_OFF   = 1,
        FLOATING_AUTO_ON    = 2,
        FLOATING_USER_ON    = 3,
    } floating;

    GQueue                      nodes;
    GQueue                      focused;
    GQueue                      allContainers;
    GQueue                      floatingWindows;

    // callbacks
    void (*OnRemoveChild) (GWMContainer*);

    enum {
        SCRATCHPAD_NONE     = 0,
        SCRATCHPAD_FRESH    = 1,
        SCRATCHPAD_CHANGED  = 2,
    } scratchpadState;

    int                         oldID;
    uint16_t                    depth;
    xcb_colormap_t              colormap;
};


#endif //GRACEFUL_WM_TYPES_H
