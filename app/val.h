//
// Created by dingjing on 23-11-27.
//

#ifndef GRACEFUL_WM_VAL_H
#define GRACEFUL_WM_VAL_H
#include "types.h"
#include "xmacro-atoms_reset.h"
#include "xmacro-atoms_NET-SUPPORTED.h"


extern bool                             gXKBSupported;
extern bool                             gShapeSupported;

extern int                              gXKBBase;
extern int                              gRandrBase;
extern int                              gShapeBase;
extern int                              gConnScreen;
extern int                              gXKBCurrentGroup;

extern xcb_window_t                     gRoot;
extern xcb_connection_t*                gConn;
extern GWMContainer*                    gFocused;
extern struct ev_loop*                  gMainLoop;
extern SnDisplay*                       gSnDisplay;
extern xcb_screen_t*                    gRootScreen;
extern xcb_timestamp_t                  gLastTimestamp;
extern GWMContainer*                    gContainerRoot;
extern xcb_atom_t                       gExtendWMHintsWindow;

extern GSList*                          gConfigModes;                   // GWMConfigMode
extern GList*                           gBindings;                      // GWMBinding
extern unsigned int                     gXCBNumLockMask;
extern GQueue                           gAllContainer;                  // GWMContainer

extern char*                            gCurConfigPath;
extern const char*                      gCurrentBindingMode;


// atom declare
#define GWM_ATOM_MACRO(atom) extern xcb_atom_t A_##atom;
GWM_NET_SUPPORTED_ATOMS_XMACRO
GWM_REST_ATOMS_XMACRO
#undef GWM_ATOM_MACRO

#endif //GRACEFUL_WM_VAL_H
