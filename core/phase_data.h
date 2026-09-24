#ifndef PHASE_DATA_H
#define PHASE_DATA_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* What a phase plane shows besides its curves, as data for a front end that
   draws it itself (docs/protocol.md "The plot as data", docs/ui-v2.md
   events 4 and 5):

   - "nullclines": a window's x- and y-nullclines as segment lists
     [x1,y1,x2,y2,...] in plot coordinates, their colours, and the frozen
     nullclines drawn with them;
   - "dfield": a window's direction field as a grid of [x,y,ux,uy] (the unit
     direction in plot coordinates) with each arrow's speed, and the
     trajectories Dir.field/flow's Flow drew there.

   Each window's record is what the core drew in it since the window was
   last blanked: nullcline.c and integrate.c report what they draw (the
   window is current_pop), the front end reports a blanked window. So an
   Erase or a redraw that no longer draws them clears them, a redraw that
   draws them again changes nothing, and the events say exactly what
   XPP's window shows. Events go out only to a client that subscribed,
   at the end of a command, one per window whose record changed since the
   one it last got.

   phase_data.cpp; C++ with a C API, nothing escapes it. */

typedef void (*PhaseDataEmit)(const char *line, size_t len);

/* the front end that sends the events; nothing is recorded before this, so
   a program without such a front end (xppaut) pays nothing */
void phase_data_init(PhaseDataEmit emit);

/* {"cmd":"data"}: the events wanted from now on (each is sent for every
   window at the next update) and whether values go as base64 float32 */
void phase_data_subscribe(int nullclines, int dfield, int f32);

/* the end of a command: the events of every window whose record changed */
void phase_data_update(void);

/* plot window pop was blanked: it shows none of this any more */
void phase_data_cleared(int pop);

/* the current window now shows these nullclines (nx and ny segments of 4
   floats, of the variables ix and iy, 1-based) in these colour indices */
void phase_data_nullclines(const float *xn, int nx, const float *yn, int ny, int ix, int iy, int xcolor,
                           int ycolor);

/* the current window's frozen nullclines: begin (none), then each set drawn */
void phase_data_frozen_begin(void);
void phase_data_frozen(const float *xn, int nx, const float *yn, int ny);

/* the current window's direction field: begin with the grid (n points a
   side, spacing du, dv in plot units), then each arrow at (x, y) with the
   vector field's components (fx, fy) there. scaled: 1 every arrow has one
   length (Scaled Dir.Fld), 0 the length follows the speed (Direct field) */
void phase_data_dfield_begin(int n, double du, double dv, int scaled, int color);
void phase_data_arrow(double x, double y, double fx, double fy);

/* Flow: flow_start before the trajectories, flow_next before each one,
   flow_stop after; in between the integrator reports each segment it draws
   in the current window with flow_step (a no-op otherwise): ncurves
   segments from (ox[i], oy[i]) to (x[i], y[i]) in colour color[i] */
void phase_data_flow_start(void);
void phase_data_flow_next(void);
void phase_data_flow_step(int ncurves, const float *ox, const float *oy, const float *x, const float *y,
                          const int *color);
void phase_data_flow_stop(void);

#ifdef __cplusplus
}
#endif
#endif
