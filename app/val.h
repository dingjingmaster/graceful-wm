//
// Created by dingjing on 23-11-27.
//

#ifndef GRACEFUL_WM_VAL_H
#define GRACEFUL_WM_VAL_H


extern xcb_window_t                     gRoot;
extern xcb_connection_t*                gConn;
extern struct ev_loop*                  gMainLoop;
extern xcb_screen_t*                    gRootScreen;
extern int                              gConnScreen;
extern xcb_timestamp_t                  gLastTimestamp;


#endif //GRACEFUL_WM_VAL_H
