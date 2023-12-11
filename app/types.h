//
// Created by dingjing on 23-11-24.
//

#ifndef GRACEFUL_WM_TYPES_H
#define GRACEFUL_WM_TYPES_H
#include <ev.h>
#include <glib.h>
#include <pcre2.h>
#include <stdint.h>
#include <stdbool.h>
#include <xcb/xcb.h>
#include <xcb/shape.h>
#include <xcb/randr.h>
#include <glib/gi18n.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <libsn/sn-launcher.h>

#if 1

#if defined(QUEUE_MACRO_DEBUG) || (defined(_KERNEL) && defined(DIAGNOSTIC))
#define _Q_INVALIDATE(a) (a) = ((void *)-1)
#else
#define _Q_INVALIDATE(a)
#endif

/*
 * Singly-linked List definitions.
 */
#define SLIST_HEAD(name, type)                      \
    struct name {                                   \
        struct type *slh_first; /* first element */ \
    }

#define SLIST_HEAD_INITIALIZER(head) \
    { NULL }

#define SLIST_ENTRY(type)                         \
    struct {                                      \
        struct type *sle_next; /* next element */ \
    }

/*
 * Singly-linked List access methods.
 */
#define SLIST_FIRST(head) ((head)->slh_first)
#define SLIST_END(head) NULL
#define SLIST_EMPTY(head) (SLIST_FIRST(head) == SLIST_END(head))
#define SLIST_NEXT(elm, field) ((elm)->field.sle_next)

#define SLIST_FOREACH(var, head, field) \
    for ((var) = SLIST_FIRST(head);     \
        (var) != SLIST_END(head);       \
        (var) = SLIST_NEXT(var, field))

#define SLIST_FOREACH_PREVPTR(var, varp, head, field) \
    for ((varp) = &SLIST_FIRST((head));               \
        ((var) = *(varp)) != SLIST_END(head);         \
        (varp) = &SLIST_NEXT((var), field))

/*
 * Singly-linked List functions.
 */
#define SLIST_INIT(head)                     \
    {                                        \
        SLIST_FIRST(head) = SLIST_END(head); \
    }

#define SLIST_INSERT_AFTER(slistelm, elm, field)            \
    do {                                                    \
        (elm)->field.sle_next = (slistelm)->field.sle_next; \
        (slistelm)->field.sle_next = (elm);                 \
    } while (0)

#define SLIST_INSERT_HEAD(head, elm, field)        \
    do {                                           \
        (elm)->field.sle_next = (head)->slh_first; \
        (head)->slh_first = (elm);                 \
    } while (0)

#define SLIST_REMOVE_NEXT(head, elm, field)                            \
    do {                                                               \
        (elm)->field.sle_next = (elm)->field.sle_next->field.sle_next; \
    } while (0)

#define SLIST_REMOVE_HEAD(head, field)                         \
    do {                                                       \
        (head)->slh_first = (head)->slh_first->field.sle_next; \
    } while (0)

#define SLIST_REMOVE(head, elm, type, field)                                 \
    do {                                                                     \
        if ((head)->slh_first == (elm)) {                                    \
            SLIST_REMOVE_HEAD((head), field);                                \
        } else {                                                             \
            struct type *curelm = (head)->slh_first;                         \
                                                                             \
            while (curelm->field.sle_next != (elm))                          \
                curelm = curelm->field.sle_next;                             \
            curelm->field.sle_next = curelm->field.sle_next->field.sle_next; \
            _Q_INVALIDATE((elm)->field.sle_next);                            \
        }                                                                    \
    } while (0)

/*
 * List definitions.
 */
#define LIST_HEAD(name, type)                      \
    struct name {                                  \
        struct type *lh_first; /* first element */ \
    }

#define LIST_HEAD_INITIALIZER(head) \
    { NULL }

#define LIST_ENTRY(type)                                              \
    struct {                                                          \
        struct type *le_next;  /* next element */                     \
        struct type **le_prev; /* address of previous next element */ \
    }

/*
 * List access methods
 */
#define LIST_FIRST(head) ((head)->lh_first)
#define LIST_END(head) NULL
#define LIST_EMPTY(head) (LIST_FIRST(head) == LIST_END(head))
#define LIST_NEXT(elm, field) ((elm)->field.le_next)

#define LIST_FOREACH(var, head, field) \
    for ((var) = LIST_FIRST(head);     \
         (var) != LIST_END(head);      \
         (var) = LIST_NEXT(var, field))

/*
 * List functions.
 */
#define LIST_INIT(head)                    \
    do {                                   \
        LIST_FIRST(head) = LIST_END(head); \
    } while (0)

#define LIST_INSERT_AFTER(listelm, elm, field)                               \
    do {                                                                     \
        if (((elm)->field.le_next = (listelm)->field.le_next) != NULL)       \
            (listelm)->field.le_next->field.le_prev = &(elm)->field.le_next; \
        (listelm)->field.le_next = (elm);                                    \
        (elm)->field.le_prev = &(listelm)->field.le_next;                    \
    } while (0)

#define LIST_INSERT_BEFORE(listelm, elm, field)           \
    do {                                                  \
        (elm)->field.le_prev = (listelm)->field.le_prev;  \
        (elm)->field.le_next = (listelm);                 \
        *(listelm)->field.le_prev = (elm);                \
        (listelm)->field.le_prev = &(elm)->field.le_next; \
    } while (0)

#define LIST_INSERT_HEAD(head, elm, field)                           \
    do {                                                             \
        if (((elm)->field.le_next = (head)->lh_first) != NULL)       \
            (head)->lh_first->field.le_prev = &(elm)->field.le_next; \
        (head)->lh_first = (elm);                                    \
        (elm)->field.le_prev = &(head)->lh_first;                    \
    } while (0)

#define LIST_REMOVE(elm, field)                                         \
    do {                                                                \
        if ((elm)->field.le_next != NULL)                               \
            (elm)->field.le_next->field.le_prev = (elm)->field.le_prev; \
        *(elm)->field.le_prev = (elm)->field.le_next;                   \
        _Q_INVALIDATE((elm)->field.le_prev);                            \
        _Q_INVALIDATE((elm)->field.le_next);                            \
    } while (0)

#define LIST_REPLACE(elm, elm2, field)                                     \
    do {                                                                   \
        if (((elm2)->field.le_next = (elm)->field.le_next) != NULL)        \
            (elm2)->field.le_next->field.le_prev = &(elm2)->field.le_next; \
        (elm2)->field.le_prev = (elm)->field.le_prev;                      \
        *(elm2)->field.le_prev = (elm2);                                   \
        _Q_INVALIDATE((elm)->field.le_prev);                               \
        _Q_INVALIDATE((elm)->field.le_next);                               \
    } while (0)

/*
 * Simple queue definitions.
 */
#define SIMPLEQ_HEAD(name, type)                                \
    struct name {                                               \
        struct type *sqh_first; /* first element */             \
        struct type **sqh_last; /* addr of last next element */ \
    }

#define SIMPLEQ_HEAD_INITIALIZER(head) \
    { NULL, &(head).sqh_first }

#define SIMPLEQ_ENTRY(type)                       \
    struct {                                      \
        struct type *sqe_next; /* next element */ \
    }

/*
 * Simple queue access methods.
 */
#define SIMPLEQ_FIRST(head) ((head)->sqh_first)
#define SIMPLEQ_END(head) NULL
#define SIMPLEQ_EMPTY(head) (SIMPLEQ_FIRST(head) == SIMPLEQ_END(head))
#define SIMPLEQ_NEXT(elm, field) ((elm)->field.sqe_next)

#define SIMPLEQ_FOREACH(var, head, field) \
    for ((var) = SIMPLEQ_FIRST(head);     \
         (var) != SIMPLEQ_END(head);      \
         (var) = SIMPLEQ_NEXT(var, field))

/*
 * Simple queue functions.
 */
#define SIMPLEQ_INIT(head)                     \
    do {                                       \
        (head)->sqh_first = NULL;              \
        (head)->sqh_last = &(head)->sqh_first; \
    } while (0)

#define SIMPLEQ_INSERT_HEAD(head, elm, field)                    \
    do {                                                         \
        if (((elm)->field.sqe_next = (head)->sqh_first) == NULL) \
            (head)->sqh_last = &(elm)->field.sqe_next;           \
        (head)->sqh_first = (elm);                               \
    } while (0)

#define SIMPLEQ_INSERT_TAIL(head, elm, field)      \
    do {                                           \
        (elm)->field.sqe_next = NULL;              \
        *(head)->sqh_last = (elm);                 \
        (head)->sqh_last = &(elm)->field.sqe_next; \
    } while (0)

#define SIMPLEQ_INSERT_AFTER(head, listelm, elm, field)                  \
    do {                                                                 \
        if (((elm)->field.sqe_next = (listelm)->field.sqe_next) == NULL) \
            (head)->sqh_last = &(elm)->field.sqe_next;                   \
        (listelm)->field.sqe_next = (elm);                               \
    } while (0)

#define SIMPLEQ_REMOVE_HEAD(head, field)                                     \
    do {                                                                     \
        if (((head)->sqh_first = (head)->sqh_first->field.sqe_next) == NULL) \
            (head)->sqh_last = &(head)->sqh_first;                           \
    } while (0)

/*
 * Tail queue definitions.
 */
#define TAILQ_HEAD(name, type)                                  \
    struct name {                                               \
        struct type *tqh_first; /* first element */             \
        struct type **tqh_last; /* addr of last next element */ \
    }

#define TAILQ_HEAD_INITIALIZER(head) \
    { NULL, &(head).tqh_first }

#define TAILQ_ENTRY(type)                                              \
    struct {                                                           \
        struct type *tqe_next;  /* next element */                     \
        struct type **tqe_prev; /* address of previous next element */ \
    }

/*
 * tail queue access methods
 */
#define TAILQ_FIRST(head) ((head)->tqh_first)
#define TAILQ_END(head) NULL
#define TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)
#define TAILQ_LAST(head, headname) \
    (*(((struct headname *)((head)->tqh_last))->tqh_last))
/* XXX */
#define TAILQ_PREV(elm, headname, field) \
    (*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#define TAILQ_EMPTY(head) \
    (TAILQ_FIRST(head) == TAILQ_END(head))

#define TAILQ_FOREACH(var, head, field) \
    for ((var) = TAILQ_FIRST(head);     \
         (var) != TAILQ_END(head);      \
         (var) = TAILQ_NEXT(var, field))

#define TAILQ_FOREACH_REVERSE(var, head, headname, field) \
    for ((var) = TAILQ_LAST(head, headname);              \
         (var) != TAILQ_END(head);                        \
         (var) = TAILQ_PREV(var, headname, field))

/*
 * Tail queue functions.
 */
#define TAILQ_INIT(head)                       \
    do {                                       \
        (head)->tqh_first = NULL;              \
        (head)->tqh_last = &(head)->tqh_first; \
    } while (0)

#define TAILQ_INSERT_HEAD(head, elm, field)                             \
    do {                                                                \
        if (((elm)->field.tqe_next = (head)->tqh_first) != NULL)        \
            (head)->tqh_first->field.tqe_prev = &(elm)->field.tqe_next; \
        else                                                            \
            (head)->tqh_last = &(elm)->field.tqe_next;                  \
        (head)->tqh_first = (elm);                                      \
        (elm)->field.tqe_prev = &(head)->tqh_first;                     \
    } while (0)

#define TAILQ_INSERT_TAIL(head, elm, field)        \
    do {                                           \
        (elm)->field.tqe_next = NULL;              \
        (elm)->field.tqe_prev = (head)->tqh_last;  \
        *(head)->tqh_last = (elm);                 \
        (head)->tqh_last = &(elm)->field.tqe_next; \
    } while (0)

#define TAILQ_INSERT_AFTER(head, listelm, elm, field)                       \
    do {                                                                    \
        if (((elm)->field.tqe_next = (listelm)->field.tqe_next) != NULL)    \
            (elm)->field.tqe_next->field.tqe_prev = &(elm)->field.tqe_next; \
        else                                                                \
            (head)->tqh_last = &(elm)->field.tqe_next;                      \
        (listelm)->field.tqe_next = (elm);                                  \
        (elm)->field.tqe_prev = &(listelm)->field.tqe_next;                 \
    } while (0)

#define TAILQ_INSERT_BEFORE(listelm, elm, field)            \
    do {                                                    \
        (elm)->field.tqe_prev = (listelm)->field.tqe_prev;  \
        (elm)->field.tqe_next = (listelm);                  \
        *(listelm)->field.tqe_prev = (elm);                 \
        (listelm)->field.tqe_prev = &(elm)->field.tqe_next; \
    } while (0)

#define TAILQ_REMOVE(head, elm, field)                                     \
    do {                                                                   \
        if (((elm)->field.tqe_next) != NULL)                               \
            (elm)->field.tqe_next->field.tqe_prev = (elm)->field.tqe_prev; \
        else                                                               \
            (head)->tqh_last = (elm)->field.tqe_prev;                      \
        *(elm)->field.tqe_prev = (elm)->field.tqe_next;                    \
        _Q_INVALIDATE((elm)->field.tqe_prev);                              \
        _Q_INVALIDATE((elm)->field.tqe_next);                              \
    } while (0)

#define TAILQ_REPLACE(head, elm, elm2, field)                                 \
    do {                                                                      \
        if (((elm2)->field.tqe_next = (elm)->field.tqe_next) != NULL)         \
            (elm2)->field.tqe_next->field.tqe_prev = &(elm2)->field.tqe_next; \
        else                                                                  \
            (head)->tqh_last = &(elm2)->field.tqe_next;                       \
        (elm2)->field.tqe_prev = (elm)->field.tqe_prev;                       \
        *(elm2)->field.tqe_prev = (elm2);                                     \
        _Q_INVALIDATE((elm)->field.tqe_prev);                                 \
        _Q_INVALIDATE((elm)->field.tqe_next);                                 \
    } while (0)

/* Swaps two consecutive elements. 'second' *MUST* follow 'first' */
#define TAILQ_SWAP(first, second, head, field)                                     \
    do {                                                                           \
        *((first)->field.tqe_prev) = (second);                                     \
        (second)->field.tqe_prev = (first)->field.tqe_prev;                        \
        (first)->field.tqe_prev = &((second)->field.tqe_next);                     \
        (first)->field.tqe_next = (second)->field.tqe_next;                        \
        if ((second)->field.tqe_next)                                              \
            (second)->field.tqe_next->field.tqe_prev = &((first)->field.tqe_next); \
        (second)->field.tqe_next = first;                                          \
        if ((head)->tqh_last == &((second)->field.tqe_next))                       \
            (head)->tqh_last = &((first)->field.tqe_next);                         \
    } while (0)

/*
 * Circular queue definitions.
 */
#define CIRCLEQ_HEAD(name, type)                    \
    struct name {                                   \
        struct type *cqh_first; /* first element */ \
        struct type *cqh_last;  /* last element */  \
    }

#define CIRCLEQ_HEAD_INITIALIZER(head) \
    {                                  \
        CIRCLEQ_END(&head)             \
        , CIRCLEQ_END(&head)           \
    }

#define CIRCLEQ_ENTRY(type)                           \
    struct {                                          \
        struct type *cqe_next; /* next element */     \
        struct type *cqe_prev; /* previous element */ \
    }

/*
 * Circular queue access methods
 */
#define CIRCLEQ_FIRST(head) ((head)->cqh_first)
#define CIRCLEQ_LAST(head) ((head)->cqh_last)
#define CIRCLEQ_END(head) ((void *)(head))
#define CIRCLEQ_NEXT(elm, field) ((elm)->field.cqe_next)
#define CIRCLEQ_PREV(elm, field) ((elm)->field.cqe_prev)
#define CIRCLEQ_EMPTY(head) \
    (CIRCLEQ_FIRST(head) == CIRCLEQ_END(head))

#define CIRCLEQ_FOREACH(var, head, field) \
    for ((var) = CIRCLEQ_FIRST(head);     \
         (var) != CIRCLEQ_END(head);      \
         (var) = CIRCLEQ_NEXT(var, field))

#define CIRCLEQ_FOREACH_REVERSE(var, head, field) \
    for ((var) = CIRCLEQ_LAST(head);              \
         (var) != CIRCLEQ_END(head);              \
         (var) = CIRCLEQ_PREV(var, field))

/*
 * Circular queue functions.
 */
#define CIRCLEQ_INIT(head)                     \
    do {                                       \
        (head)->cqh_first = CIRCLEQ_END(head); \
        (head)->cqh_last = CIRCLEQ_END(head);  \
    } while (0)

#define CIRCLEQ_INSERT_AFTER(head, listelm, elm, field)        \
    do {                                                       \
        (elm)->field.cqe_next = (listelm)->field.cqe_next;     \
        (elm)->field.cqe_prev = (listelm);                     \
        if ((listelm)->field.cqe_next == CIRCLEQ_END(head))    \
            (head)->cqh_last = (elm);                          \
        else                                                   \
            (listelm)->field.cqe_next->field.cqe_prev = (elm); \
        (listelm)->field.cqe_next = (elm);                     \
    } while (0)

#define CIRCLEQ_INSERT_BEFORE(head, listelm, elm, field)       \
    do {                                                       \
        (elm)->field.cqe_next = (listelm);                     \
        (elm)->field.cqe_prev = (listelm)->field.cqe_prev;     \
        if ((listelm)->field.cqe_prev == CIRCLEQ_END(head))    \
            (head)->cqh_first = (elm);                         \
        else                                                   \
            (listelm)->field.cqe_prev->field.cqe_next = (elm); \
        (listelm)->field.cqe_prev = (elm);                     \
    } while (0)

#define CIRCLEQ_INSERT_HEAD(head, elm, field)          \
    do {                                               \
        (elm)->field.cqe_next = (head)->cqh_first;     \
        (elm)->field.cqe_prev = CIRCLEQ_END(head);     \
        if ((head)->cqh_last == CIRCLEQ_END(head))     \
            (head)->cqh_last = (elm);                  \
        else                                           \
            (head)->cqh_first->field.cqe_prev = (elm); \
        (head)->cqh_first = (elm);                     \
    } while (0)

#define CIRCLEQ_INSERT_TAIL(head, elm, field)         \
    do {                                              \
        (elm)->field.cqe_next = CIRCLEQ_END(head);    \
        (elm)->field.cqe_prev = (head)->cqh_last;     \
        if ((head)->cqh_first == CIRCLEQ_END(head))   \
            (head)->cqh_first = (elm);                \
        else                                          \
            (head)->cqh_last->field.cqe_next = (elm); \
        (head)->cqh_last = (elm);                     \
    } while (0)

#define CIRCLEQ_REMOVE(head, elm, field)                                   \
    do {                                                                   \
        if ((elm)->field.cqe_next == CIRCLEQ_END(head))                    \
            (head)->cqh_last = (elm)->field.cqe_prev;                      \
        else                                                               \
            (elm)->field.cqe_next->field.cqe_prev = (elm)->field.cqe_prev; \
        if ((elm)->field.cqe_prev == CIRCLEQ_END(head))                    \
            (head)->cqh_first = (elm)->field.cqe_next;                     \
        else                                                               \
            (elm)->field.cqe_prev->field.cqe_next = (elm)->field.cqe_next; \
        _Q_INVALIDATE((elm)->field.cqe_prev);                              \
        _Q_INVALIDATE((elm)->field.cqe_next);                              \
    } while (0)

#define CIRCLEQ_REPLACE(head, elm, elm2, field)                                    \
    do {                                                                           \
        if (((elm2)->field.cqe_next = (elm)->field.cqe_next) == CIRCLEQ_END(head)) \
            (head)->cqh_last = (elm2);                                             \
        else                                                                       \
            (elm2)->field.cqe_next->field.cqe_prev = (elm2);                       \
        if (((elm2)->field.cqe_prev = (elm)->field.cqe_prev) == CIRCLEQ_END(head)) \
            (head)->cqh_first = (elm2);                                            \
        else                                                                       \
            (elm2)->field.cqe_prev->field.cqe_next = (elm2);                       \
        _Q_INVALIDATE((elm)->field.cqe_prev);                                      \
        _Q_INVALIDATE((elm)->field.cqe_next);                                      \
    } while (0)


#define FREE(x) \
{                       \
    if (x) free (x);    \
    x = NULL;           \
}

#define GREP_FIRST(dest, head, condition) \
    NODES_FOREACH (head) {                \
        if (!(condition))                 \
            continue;                     \
                                          \
        (dest) = child;                   \
        break;                            \
    }

#define die(...) errx(EXIT_FAILURE, __VA_ARGS__);
#define exit_if_null(pointer, ...) \
    {                              \
        if (pointer == NULL)       \
            die(__VA_ARGS__);      \
    }
#define STARTS_WITH(string, needle) (strncasecmp((string), (needle), strlen((needle))) == 0)
#define CIRCLEQ_NEXT_OR_NULL(head, elm, field) (CIRCLEQ_NEXT(elm, field) != CIRCLEQ_END(head) ? CIRCLEQ_NEXT(elm, field) : NULL)
#define CIRCLEQ_PREV_OR_NULL(head, elm, field) (CIRCLEQ_PREV(elm, field) != CIRCLEQ_END(head) ? CIRCLEQ_PREV(elm, field) : NULL)

#define NODES_FOREACH(head) \
    for (GWMContainer* child = (GWMContainer *)-1; (child == (GWMContainer *)-1) && ((child = 0), true);) \
        TAILQ_FOREACH (child, &((head)->nodesHead), nodes)

#define NODES_FOREACH_REVERSE(head)                                            \
    for (GWMContainer *child = (GWMContainer *)-1; (child == (GWMContainer *)-1) && ((child = 0), true);) \
        TAILQ_FOREACH_REVERSE (child, &((head)->nodesHead), nodesHead, nodes)

#define CALL(obj, member, ...) obj->member(obj, ##__VA_ARGS__)

#define SWAP(first, second, type) \
    do {                          \
        type tmpSWAP = first;     \
        first = second;           \
        second = tmpSWAP;         \
    } while (0)

#define CAIRO_SURFACE_FLUSH(surface)  \
    do {                              \
        cairo_surface_flush(surface); \
        cairo_surface_flush(surface); \
    } while (0)

#ifdef g_return_val_if_fail
#undef g_return_val_if_fail
#endif
#define g_return_val_if_fail(expr, val) \
  G_STMT_START { \
    if (G_LIKELY (expr)) \
      { } \
    else \
      { return (val); } \
  } G_STMT_END

#ifdef g_return_if_fail
#undef g_return_if_fail
#endif
#define g_return_if_fail(expr) \
  G_STMT_START { \
    if (G_LIKELY (expr)) \
      { } \
    else \
      { return; } \
  } G_STMT_END

#define EXIT_IF_MEM_IS_NULL(p) \
  G_STMT_START { \
    if (G_UNLIKELY (!p)) { \
      ERROR("mem " #p " is null"); \
      exit(-1); \
    } \
  } G_STMT_END

#endif



typedef uint32_t                            GWMEventStateMask;

typedef enum Layout                         GWMLayout;                  // ok
typedef enum Broder                         GWMBorder;                  // ok
typedef enum Cursor                         GWMCursor;                  // ok
typedef enum Warping                        GWMWarping;
typedef enum Position                       GWMPosition;                // ok
typedef enum GapsMask                       GWMGapsMask;                // ok
typedef enum MarkMode                       GWMMarkMode;                // ok
typedef enum Adjacent                       GWMAdjacent;                // ok
typedef enum SmartGaps                      GWMSmartGaps;               // ok
typedef enum InputType                      GWMInputType;               // ok
typedef enum Direction                      GWMDirection;               // ok
typedef enum ConfigLoad                     GWMConfigLoad;              // ok
typedef enum DragResult                     GWMDragResult;              // ok
typedef enum KillWindow                     GWMKillWindow;              // ok
typedef enum TilingDrag                     GWMTilingDrag;              // ok
typedef enum Orientation                    GWMOrientation;             // ok
typedef enum BorderStyle                    GWMBorderStyle;             // ok
typedef enum XKBGroupMask                   GWMXKBGroupMask;            // ok
typedef enum FocusWarping                   GWMFocusWarping;            // ok
typedef enum SmartBorders                   GWMSmartBorders;            // ok
typedef enum OutputCloseFar                 GWMOutputCloseFar;          // ok
typedef enum FullScreenMode                 GWMFullScreenMode;          // ok
typedef enum HideEdgeBordersMode            GWMHideEdgeBordersMode;     // ok

typedef struct Gaps                         GWMGaps;                    // ok
typedef struct Size                         GWMSize;                    // ok
typedef struct Rect                         GWMRect;                    // ok
typedef struct Font                         GWMFont;                    // ok
typedef struct Mark                         GWMMark;                    // ok
typedef struct Regex                        GWMRegex;                   // ok
typedef struct Match                        GWMMatch;                   // ok
typedef struct Color                        GWMColor;                   // ok
typedef struct Output                       GWMOutput;                  // ok
typedef struct Config                       GWMConfig;                  // ok
typedef struct Window                       GWMWindow;                  // ok
typedef struct Surface                      GWMSurface;                 // ok
typedef struct Binding                      GWMBinding;                 // ok
typedef struct Container                    GWMContainer;               // ok
typedef struct Colortriple                  GWMColoriple;               // ok
typedef struct OutputHead                   GWMOutputHead;              // ok
typedef struct ColorPixel                   GWMColorPixel;              // ok
typedef struct OutputName                   GWMOutputName;              // ok
typedef struct Assignment                   GWMAssignment;              // ok
typedef struct RenderParams                 GWMRenderParams;            // ok
typedef struct CommandResult                GWMCommandResult;           // ok
typedef struct BindingKeycode               GWMBindingKeycode;          // ok
typedef struct AssignmentHead               GWMAssignmentHead;          // ok
typedef struct StartupSequence              GWMStartupSequence;         // ok
typedef struct AllContainerHead             GWMAllContainerHead;        // ok
typedef struct ReserveEdgePixels            GWMReserveEdgePixels;       // ok
typedef struct WorkspaceAssignment          GWMWorkspaceAssignment;     // ok
typedef struct DecorationRenderParams       GWMDecorationRenderParams;  // ok
typedef struct WorkspaceAssignmentsHead     GWMWorkspaceAssignmentsHead;// ok

typedef struct ConfigMode                   GWMConfigMode;              // ok
typedef struct ConfigContext                GWMConfigContext;           // ok

TAILQ_HEAD(OutputHead, Output);
TAILQ_HEAD(AssignmentHead, Assignment);
TAILQ_HEAD(AllContainerHead, Container);
TAILQ_HEAD(WorkspaceAssignmentsHead, WorkspaceAssignment);

enum Warping
{
    POINTER_WARPING_OUTPUT = 0,
    POINTER_WARPING_NONE = 1
};

enum SmartGaps
{
    SMART_GAPS_OFF,
    SMART_GAPS_ON,
    SMART_GAPS_INVERSE_OUTER
};

enum SmartBorders
{
    SMART_BORDERS_OFF,
    SMART_BORDERS_ON,
    SMART_BORDERS_NO_GAPS
};

enum TilingDrag
{
    TILING_DRAG_OFF = 0,
    TILING_DRAG_MODIFIER = 1,
    TILING_DRAG_TITLEBAR = 2,
    TILING_DRAG_MODIFIER_OR_TITLEBAR = 3
};

enum OutputCloseFar
{
    CLOSEST_OUTPUT = 0,
    FARTHEST_OUTPUT = 1
};

enum InputType
{
    B_KEYBOARD      = 0,
    B_MOUSE         = 1,
};

enum MarkMode
{
    MM_REPLACE,
    MM_ADD
};

enum Orientation
{
    NO_ORIENTATION = 0,
    HORIZON,
    VERT
};

enum Position
{
    BEFORE,
    AFTER
};

enum Direction
{
    D_LEFT,
    D_RIGHT,
    D_UP,
    D_DOWN
};

enum BorderStyle
{
    BS_NONE         = 0,
    BS_PIXEL        = 1,
    BS_NORMAL       = 2,
};

enum XKBGroupMask
{
    GWM_XKB_GROUP_MASK_ANY  = 0,
    GWM_XKB_GROUP_MASK_1    = (1 << 0),
    GWM_XKB_GROUP_MASK_2    = (1 << 1),
    GWM_XKB_GROUP_MASK_3    = (1 << 2),
    GWM_XKB_GROUP_MASK_4    = (1 << 3)
};

enum FullScreenMode
{
    CF_NONE         = 0,
    CF_OUTPUT       = 1,
    CF_GLOBAL       = 2,
};

enum ConfigLoad
{
    C_VALIDATE,
    C_LOAD,
    C_RELOAD,
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

enum Cursor
{
    CURSOR_POINTER = 0,
    CURSOR_RESIZE_HORIZONTAL,
    CURSOR_RESIZE_VERTICAL,
    CURSOR_TOP_LEFT_CORNER,
    CURSOR_TOP_RIGHT_CORNER,
    CURSOR_BOTTOM_LEFT_CORNER,
    CURSOR_BOTTOM_RIGHT_CORNER,
    CURSOR_WATCH,
    CURSOR_MOVE,
    CURSOR_MAX,
};

enum Adjacent
{
    ADJ_NONE                = 0,
    ADJ_LEFT_SCREEN_EDGE    = (1 << 0),
    ADJ_RIGHT_SCREEN_EDGE   = (1 << 1),
    ADJ_UPPER_SCREEN_EDGE   = (1 << 2),
    ADJ_LOWER_SCREEN_EDGE   = (1 << 4)
};

enum Broder
{
    BORDER_LEFT             = (1 << 0),
    BORDER_RIGHT            = (1 << 1),
    BORDER_TOP              = (1 << 2),
    BORDER_BOTTOM           = (1 << 3)
};

enum DragResult
{
    DRAGGING = 0,
    DRAG_SUCCESS,
    DRAG_REVERT,
    DRAG_ABORT
};

enum HideEdgeBordersMode
{
    HEBM_NONE = ADJ_NONE,
    HEBM_VERTICAL = ADJ_LEFT_SCREEN_EDGE | ADJ_RIGHT_SCREEN_EDGE,
    HEBM_HORIZONTAL = ADJ_UPPER_SCREEN_EDGE | ADJ_LOWER_SCREEN_EDGE,
    HEBM_BOTH = HEBM_VERTICAL | HEBM_HORIZONTAL,
    HEBM_SMART = (1 << 5),
    HEBM_SMART_NO_GAPS = (1 << 6)
};

enum FocusWarping
{
    FOCUS_WRAPPING_OFF = 0,
    FOCUS_WRAPPING_ON = 1,
    FOCUS_WRAPPING_FORCE = 2,
    FOCUS_WRAPPING_WORKSPACE = 3
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

struct CommandResult
{
    bool                                        parseError;
    char*                                       errorMessage;
    bool                                        needsTreeRender;
};

struct Mark
{
    char*                                       name;
    TAILQ_ENTRY(Mark)                           marks;
};

struct ColorPixel
{
    char                                        hex[8];
    uint32_t                                    pixel;
    SLIST_ENTRY(ColorPixel)                     colorPixels;
};

struct BindingKeycode
{
    xcb_keycode_t                               keycode;
    GWMEventStateMask                           modifiers;
    TAILQ_ENTRY(BindingKeycode)                 keycodes;
};

struct Colortriple
{
    GWMColor border;
    GWMColor background;
    GWMColor text;
    GWMColor indicator;
    GWMColor childBorder;
};

struct Font
{
    enum {
        FONT_TYPE_NONE = 0,
        FONT_TYPE_XCB,
        FONT_TYPE_PANGO
    } type;

    int                                         height;    // The height of the font, built from font_ascent + font_descent
    char*                                       pattern;   // The pattern/name used to load the font.

    union {
        struct {
            xcb_font_t              id;                     // The xcb-id for the font
            xcb_query_font_reply_t* info;                   // Font information gathered from the server
            xcb_charinfo_t*         table;                  // Font table for this font (may be NULL)
        } xcb;
        PangoFontDescription*       pangoDesc;              // The pango font description
    } specific;
};

struct Config
{
    bool                                        showMarks;
    bool                                        forceXinerama;
    bool                                        disableRandr15;
    bool                                        disableWorkspaceBar;
    bool                                        disableFocusFollowsMouse;
    bool                                        workspaceAutoBackAndForth;

    int                                         containerStackLimit;
    int                                         containerStackLimitValue;
    int                                         defaultBorderWidth;
    int                                         defaultFloatingBorderWidth;
    int                                         defaultOrientation;
    int                                         numberBarconfigs;   // The number of currently parsed barconfigs

    const char*                                 terminal;
    char*                                       fakeOutputs;
    char*                                       ipcSocketPath;
    char*                                       restartStatePath;

    float                                       workspaceUrgencyTimer;

    enum {
        FOWA_SMART,     // Focus if the target workspace is visible, set urgency hint otherwise.
        FOWA_URGENT,    // Always set the urgency hint.
        FOWA_FOCUS,     // Always focus the window.
        FOWA_NONE       // Ignore the request (no focus, no urgency hint).
    } focusOnWindowActivation;

    enum {
        ALIGN_LEFT,
        ALIGN_CENTER,
        ALIGN_RIGHT
    } titleAlign;

    GWMBorderStyle                              defaultBorder;
    GWMBorderStyle                              defaultFloatingBorder;

    uint32_t                                    floatingModifier;
    int32_t                                     floatingMaximumWidth;
    int32_t                                     floatingMaximumHeight;
    int32_t                                     floatingMinimumWidth;
    int32_t                                     floatingMinimumHeight;

    /* Color codes are stored here */
    struct ConfigClient {
        GWMColor            background;
        struct Colortriple  focused;
        struct Colortriple  focused_inactive;
        struct Colortriple  focused_tab_title;
        struct Colortriple  unfocused;
        struct Colortriple  urgent;
        struct Colortriple  placeholder;
        bool                gotFocusedTabTitle;
    } client;
    struct ConfigBar {
        struct Colortriple  focused;
        struct Colortriple  unfocused;
        struct Colortriple  urgent;
    } bar;

    enum {
        PDF_SMART = 0,              // display (and focus) the popup when it belongs to the fullscreen window only.
        PDF_LEAVE_FULLSCREEN = 1,   // leave fullscreen mode unconditionally
        PDF_IGNORE = 2,             // just ignore the popup, that is, don’t map it
    } popupDuringFullscreen;

    GWMGaps                                     gaps;               // Gap sizes
    GWMFont                                     font;
    GWMSmartGaps                                smartGaps;          // Disable gaps if there is only one container on the workspace
    GWMTilingDrag                               tilingDrag;
    GWMWarping                                  mouseWarping;
    GWMSmartBorders                             smartBorders;       // Should single containers on a workspace receive a border?
    GWMLayout                                   defaultLayout;
    GWMFocusWarping                             focusWrapping;
    GWMHideEdgeBordersMode                      hideEdgeBorders;    //
};

struct RenderParams
{
    int                                         x;
    int                                         y;
    int                                         decoHeight;
    GWMRect                                     rect;
    int                                         children;
    int*                                        sizes;
};

struct ReserveEdgePixels
{
    uint32_t                                    left;
    uint32_t                                    right;
    uint32_t                                    top;
    uint32_t                                    bottom;
};

struct OutputName
{
    char*                                       name;
    SLIST_ENTRY(OutputName)                     names;
};

struct ConfigContext
{
    bool                                        hasErrors;
    bool                                        hasWarnings;
    int                                         lineNumber;
    char*                                       lineCopy;
    const char*                                 filename;
    char*                                       compactError;
    int                                         firstColumn;
    int                                         lastColumn;
};

struct ConfigMode
{
    char*                                       name;
    bool                                        pangoMarkup;
    struct BindingsHead*                        bindings;
    SLIST_ENTRY(ConfigMode)                     modes;
};

struct Output
{
    xcb_randr_output_t                          id;
    GWMRect                                     rect;
    bool                                        active;
    bool                                        changed;
    bool                                        primary;
    TAILQ_ENTRY(Output)                         outputs;
    SLIST_HEAD(namesHead, OutputName)           namesHead;
    GWMContainer*                               container;
    bool                                        toBeDisabled;
};

struct StartupSequence
{
    char*                                       id;                // startup ID for this sequence, generated by libstartup-notification
    char*                                       workspace;         // workspace on which this startup was initiated
    SnLauncherContext*                          context;           // libstartup-notification context for this launch
    time_t                                      deleteAt;          // time at which this sequence should be deleted (after it was marked as completed)
    GQueue                                      sequences;
};


struct DecorationRenderParams
{
    struct Colortriple*                         color;
    int                                         borderStyle;
    GWMSize                                     containerSize;
    GWMSize                                     containerWindowSize;
    GWMRect                                     containerDecorationRect;
    GWMColor                                    background;
    GWMLayout                                   parentLayout;
    bool                                        containerIsLeaf;
};

struct Match
{
    char*                                       error;
    GWMRegex*                                   mark;
    GWMRegex*                                   title;
    GWMRegex*                                   class;
    GWMRegex*                                   machine;
    GWMRegex*                                   instance;
    GWMRegex*                                   workspace;
    GWMRegex*                                   windowRole;
    GWMRegex*                                   application;
    xcb_atom_t                                  windowType;
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
    xcb_window_t                                id;
    enum {
        WM_ANY          = 0,
        WM_TILING_AUTO,
        WM_TILING_USER,
        WM_TILING,
        WM_FLOATING_AUTO,
        WM_FLOATING_USER,
        WM_FLOATING
    } windowMode;
    GWMContainer*                               containerID;
    bool                                        matchAllWindows;

    enum {
        M_HERE          = 0,
        M_ASSIGN_WS,
        M_BELOW
    } insertWhere;

    TAILQ_ENTRY(Match)                          matches;
    bool                                        restartMode;
};

struct WorkspaceAssignment
{
    char*                                       name;
    char*                                       output;
    GWMGaps                                     gaps;
    GWMGapsMask                                 gapsMask;

    TAILQ_ENTRY(WorkspaceAssignment)            wsAssignments;
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
    } type;
    GWMMatch                                    match;
    union {
        char*       command;
        char*       workspace;
        char*       output;
    } destination;
    TAILQ_ENTRY(Assignment)                     assignments;
};

struct Binding
{
    GWMInputType                                inputType;

    enum {
        B_UPON_KEYPRESS = 0,                            // This binding will only be executed upon KeyPress events
        B_UPON_KEYRELEASE = 1,                          // This binding will be executed either upon a KeyRelease event, or…
        B_UPON_KEYRELEASE_IGNORE_MODS = 2,
    } release;

    bool                                        border;
    bool                                        wholeWindow;
    bool                                        excludeTitleBar;
    uint32_t                                    keycode;
    GWMEventStateMask                           eventStateMask;
    char*                                       symbol;
    char*                                       command;
    TAILQ_ENTRY(Binding)                        bindings;
    TAILQ_HEAD(keycodesHead, BindingKeycode)    keycodesHead;
};

struct Window
{
    xcb_window_t                                id;
    xcb_window_t                                leader;                 // 保存leader窗口(工具窗口和类似浮动窗口的逻辑父窗口)ID
    xcb_window_t                                transientFor;           //
    uint32_t                                    nrAssignments;          // 指向已经在此窗口运行过的赋值的指针(赋值只运行一次)
    GWMAssignment**                             ranAssignments;         //

    char*                                       classClass;
    char*                                       classInstance;
    char*                                       name;

    char*                                       role;                   // WM_WINDOW_ROLE of this window
    char*                                       machine;                // WM_CLIENT_MACHINE of the window
    bool                                        nameXChanged;           //
    bool                                        usesNetWMName;          //
    bool                                        needTakeFocus;          //
    bool                                        doesNotAcceptFocus;     //
    xcb_atom_t                                  windowType;
    uint32_t                                    wmDesktop;
    enum {
        W_NO_DOCK       = 0,
        W_DOCK_TOP      = 1,
        W_DOCK_BOTTOM   =2,
    } dock;
    struct timeval                              urgent;                 // Window何时被标记为紧急? 0表示不紧急
    GWMReserveEdgePixels                        reserved;               //

    uint16_t                                    depth;

    int                                         baseWidth;
    int                                         baseHeight;

    int                                         widthInc;
    int                                         heightInc;

    int                                         minWidth;
    int                                         minHeight;

    int                                         maxWidth;
    int                                         maxHeight;

    double                                      minAspectRatio;
    double                                      maxAspectRatio;

    cairo_surface_t*                            icon;

    bool                                        shaped;
    bool                                        inputShaped;
    time_t                                      managedSince;
    bool                                        swallowed;
};

struct Surface
{
    bool                                        ownsGC;
    int                                         width;
    int                                         height;
    xcb_drawable_t                              id;             // The drawable which is being represented
    xcb_gcontext_t                              gc;             // XCB graphics context
    cairo_t*                                    cairo;
    cairo_surface_t*                            surface;        // A cairo surface representing the drawable
};


struct Container
{
    bool                                        mapped;
    bool                                        urgent;             // 是否加急处理，如果容器内有窗口设置加急则设置
    bool                                        pixmapRecreated;    //
    uint8_t                                     ignoreUnmap;        // 该容器应该忽略的UnmapNotify事件的数量(如果UnmapNotify事件是由本身引起的，则需要忽略这些事件。例如，在重新定位或在工作区更改时取消对窗口的映射时。)
    GWMSurface                                  frame;
    GWMSurface                                  frameBuffer;

    enum {
        CT_ROOT = 0,
        CT_OUTPUT = 1,
        CT_CON = 2,
        CT_FLOATING_CON = 3,
        CT_WORKSPACE = 4,
        CT_DOCK_AREA = 5,
    } type;

    int                                         workspaceNum;
    GWMGaps                                     gaps;
    GWMContainer*                               parent;
    GWMRect                                     rect;
    GWMRect                                     windowRect;
    GWMRect                                     decorationRect;
    GWMRect                                     geoRect;

    char*                                       name;
    char*                                       titleFormat;

    int                                         windowIconPadding;
    char*                                       stickyGroup;                // sticky group 它将多个容器捆绑到一个组中。内容在所有容器之间共享，也就是说，它们显示在当前可见的任何一个容器上

    bool                                        markChanged;                // 缓存以确定是否需要重新绘制
    TAILQ_HEAD(marksHead, Mark)                 marksHead;                  // 用户可定义标记，以便稍后跳转到此容器
    double                                      percent;

    int                                         borderWidth;
    int                                         currentBorderWidth;

    GWMWindow*                                  window;                     //
    struct ev_timer*                            urgencyTimer;
    GWMDecorationRenderParams*                  decorationRenderParams;
    TAILQ_HEAD(nodesHead, Container)            nodesHead;
    TAILQ_HEAD(focusHead, Container)            focusHead;
    TAILQ_HEAD(swallowHead, Match)              swallowHead;
    TAILQ_HEAD(floatingHead, Container)         floatingHead;


    GWMFullScreenMode                           fullScreenMode;

    bool                                        sticky;

    GWMLayout                                   layout;
    GWMLayout                                   lastSplitLayout;
    GWMLayout                                   workspaceLayout;

    GWMBorderStyle                              borderStyle;
    GWMBorderStyle                              maxUserBorderStyle;

    enum {
        FLOATING_AUTO_OFF   = 0,
        FLOATING_USER_OFF   = 1,
        FLOATING_AUTO_ON    = 2,
        FLOATING_USER_ON    = 3,
    } floating;

    TAILQ_ENTRY(Container)                      nodes;
    TAILQ_ENTRY(Container)                      focused;
    TAILQ_ENTRY(Container)                      allContainers;
    TAILQ_ENTRY(Container)                      floatingWindows;

    // callbacks
    void (*onRemoveChild) (GWMContainer*);

    enum {
        SCRATCHPAD_NONE     = 0,
        SCRATCHPAD_FRESH    = 1,
        SCRATCHPAD_CHANGED  = 2,
    } scratchpadState;

    int                                         oldID;
    uint16_t                                    depth;
    xcb_colormap_t                              colormap;
};


#endif //GRACEFUL_WM_TYPES_H
