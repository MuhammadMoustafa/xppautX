#pragma once

/*
 * Opaque window handle stored inside core data structures (GRAPH, CURVE,
 * BROWSER, ...). The numerics never dereference it; a front end may store
 * whatever fits, or leave it zero.
 */
typedef unsigned long XppWinId;

/* The protocol reserves this range for non-plot surfaces; plot IDs skip it. */
#define WIN_AUTO 101
#define WIN_ANI 104
#define WIN_APLOT 105

