#ifndef PHASE_DATA_H
#define PHASE_DATA_H

#include <array>
#include <string_view>
#include <vector>
#include "many_pops.h"

namespace xpp {
struct Session; /* session.h */

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
   window is plot_windows.active), the front end reports a blanked window. So an
   Erase or a redraw that no longer draws them clears them, a redraw that
   draws them again changes nothing, and the events say exactly what
   XPP's window shows. Events go out only to a client that subscribed,
   at the end of a command, one per window whose record changed since the
   one it last got.

   phase_data.cpp; nothing escapes it. */

/* What each plot window shows of these, recorded since it was last blanked:
   a Session's (Session::phase_shown), which phase_data.cpp fills; what the
   client got of it is phase_data.cpp's own, the client's. */
struct PhaseShown {
    /* nullcline segments, 4 values each */
    struct Clines {
        std::vector<float> x, y;
    };
    struct Nullclines {
        int ix = 0, iy = 0, xcolor = 0, ycolor = 0;
        Clines now;
        std::vector<Clines> frozen;
    };
    /* one curve's trajectories of Flow */
    struct FlowCurve {
        int color = 0;
        std::vector<float> x, y; /* the trajectories one after the other, NaN between two */
        /* thinning: the last point kept, and the last one seen when it was not kept */
        float kx = 0, ky = 0, px = 0, py = 0;
        bool pending = false;
    };
    struct Field {
        int n = 0, scaled = 0, color = 0;
        double du = 0, dv = 0;
        std::vector<float> grid;  /* x, y, ux, uy per arrow */
        std::vector<float> speed; /* one per arrow */
        std::vector<FlowCurve> flows; /* one per curve of the window */
    };
    struct Window {
        Nullclines nc;
        Field df;
        /* every change of nc, of df, bumps its generation: an update looks
           at a record again only when its generation moved */
        unsigned long nc_generation = 0, df_generation = 0;
        unsigned long trajectory = 0; /* the flow trajectory its last point was from */
    };
    std::array<Window, MAXPOP> windows;
    /* Flow is drawing (phase_data_flow_start to _stop), and the number of
       the trajectory it draws */
    bool flowing = false;
    unsigned long trajectory = 0;
};

typedef void (*PhaseDataEmit)(std::string_view line);

/* the front end that sends the events; nothing is recorded before this, so
   a program without such a front end (xppaut) pays nothing */
void phase_data_init(PhaseDataEmit emit);

/* {"cmd":"data"}: the events wanted from now on (each is sent for every
   window at the next update) and whether values go as base64 float32 */
void phase_data_subscribe(int nullclines, int dfield, int f32);

/* plot window pop was blanked: it shows none of this any more */
void phase_data_cleared(Session &s, int pop);

/* Flow: flow_start before the trajectories, flow_next before each one,
   flow_stop after; in between the integrator reports each segment it draws
   with phase_data_flow_step (below) */
void phase_data_flow_start(Session &s);
void phase_data_flow_next(Session &s);
void phase_data_flow_stop(Session &s);

/* the end of a command on s: the events of every window whose record
   changed */
void phase_data_update(Session &s);

/* The recorders below record into s.phase_shown's record of the active
   plot window of s, the one drawn in. */

/* that window now shows these nullclines (nx and ny segments of 4 floats,
   of the variables ix and iy, 1-based) in these colour indices */
void phase_data_nullclines(Session &s, const float *xn, int nx, const float *yn, int ny, int ix, int iy,
                           int xcolor, int ycolor);

/* its frozen nullclines: begin (none), then each set drawn */
void phase_data_frozen_begin(Session &s);
void phase_data_frozen(Session &s, const float *xn, int nx, const float *yn, int ny);

/* its direction field: begin with the grid (n points a side, spacing du,
   dv in plot units), then each arrow at (x, y) with the vector field's
   components (fx, fy) there. scaled: 1 every arrow has one length (Scaled
   Dir.Fld), 0 the length follows the speed (Direct field) */
void phase_data_dfield_begin(Session &s, int n, double du, double dv, int scaled, int color);
void phase_data_arrow(Session &s, double x, double y, double fx, double fy);

/* between flow_start and flow_stop, each segment of Flow the integrator
   draws (a no-op otherwise): ncurves segments from (ox[i], oy[i]) to
   (x[i], y[i]) in colour color[i] */
void phase_data_flow_step(Session &s, int ncurves, const float *ox, const float *oy, const float *x,
                          const float *y, const int *color);

} // namespace xpp
#endif
