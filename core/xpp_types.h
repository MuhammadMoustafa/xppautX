#ifndef XPP_TYPES_H
#define XPP_TYPES_H

/*
 * Opaque window handle stored inside core data structures (GRAPH, CURVE,
 * BROWSER, ...). The numerics never dereference it; a front end may store
 * whatever fits, or leave it zero.
 */
typedef unsigned long XppWinId;

#endif
