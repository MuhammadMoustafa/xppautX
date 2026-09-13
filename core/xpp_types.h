#ifndef XPP_TYPES_H
#define XPP_TYPES_H

/*
 * Opaque window handle stored inside core data structures (GRAPH, CURVE,
 * BROWSER, ...). The X11 front end keeps an X11 Window in it, which is an
 * XID, i.e. an unsigned long, so the two types are interchangeable there.
 * The numerics never dereference it; a non-X11 front end may store anything
 * that fits, or leave it zero.
 */
typedef unsigned long XppWinId;

#endif
