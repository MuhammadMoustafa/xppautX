#ifndef AUTO_DATA_H
#define AUTO_DATA_H
#include <stddef.h>

#include <string>
#include <vector>

/* AUTO's info strip and stability circle as data, for a front end that
   draws them itself: the "autoinfo" event (docs/protocol.md "The AUTO
   diagram as data", docs/ui-v2.md event 7).

   - The info strip (window 103 of the X11 program) is what
     traverse_out() in auto_nox.c shows for the point a grab's cursor is on:
     branch, point, type, label, the parameters, the norm, the plotted
     variable's value and the period.
   - The stability circle (window 102) holds a stored diagram point's
     stability values (auto_stability.h): for a steady state e^lambda of
     each eigenvalue lambda (inside the unit circle is stable), for a
     periodic orbit its Floquet multipliers, all zeros where AUTO did not
     compute them (a run's first point, unless it restarts from a label of
     the same kind). Every point AUTO stores shows it, so after a run it
     is the last point's, while grabbing the cursor's; a redraw of the
     diagram leaves the strip and the circle as they were.

   - Why the run's last branch ended (auto_stop.h, T23): "stop", null
     until a branch ends; a run's start and a new AUTO window clear it.

   auto_nox.c reports the strip and the circle as it draws them; the front
   end sends the event at the end of a command, before every prompt and at
   most ten times a second while AUTO runs, and only when it differs from the one it sent
   last. Nothing is recorded before auto_data_init(), so a program without
   such a front end (xppaut) pays nothing.

   auto_data.cpp; nothing escapes it. */

namespace xpp {

struct Session; /* session.h */

typedef void (*AutoDataEmit)(const char *line, size_t len);

/* the index in the diagram data the client holds of AUTO's diagram entry
   `node` (DIAGRAM.index), or -1 when the data do not have it */
typedef int (*AutoDataPointOf)(int node);

void auto_data_init(AutoDataEmit emit, AutoDataPointOf point_of);

/* {"cmd":"data"} with or without "autoinfo": the event is sent at the next
   update whatever it holds */
void auto_data_subscribe(int on);

/* a new AUTO window, or none: nothing to show (no stop reason either) */
void auto_data_forget(Session &s);

/* the point the info strip shows; names are the model's, whole. p2name is
   NULL for a one-parameter point (the strip shows a blank name and 0) */
struct AutoDataInfo {
    int ibr, pt;     /* AUTO's branch and point number, signed as AUTO has them */
    int itp, lab;    /* AUTO's point type and label */
    int type;        /* 1 stable steady state, 2 unstable, 3 stable periodic, 4 unstable periodic */
    int flag2;       /* two-parameter curve kind, 0 for one parameter */
    int node;        /* DIAGRAM.index of the point */
    const char *sym; /* auto_bif_sym: EP, LP, HB, ... or blank */
    const char *p1name, *p2name;
    double p1, p2;
    double norm;
    const char *vname; /* the variable of the Axes setting and its value (u0) */
    double u;
    double per;
    double x, y, y2; /* where the diagram plots it (auto_xy_plot) */
};

/* what the strip and the circle of a Session show (AutoState::shown) */
struct AutoDataShown {
    AutoDataInfo v{}; /* its names in the strings below */
    std::string sym, p1name, p2name, vname;
    bool two = false;
    bool has_info = false, has_stab = false, stab_periodic = false;
    std::vector<double> stab_re, stab_im;
    int held = 0; /* auto_data_hold depth */
};

void auto_data_info(Session &s, const AutoDataInfo *info);

/* the stability circle now shows these n values (evr + i evi); periodic:
   Floquet multipliers, else e^lambda of a steady state's eigenvalues */
void auto_data_stab(Session &s, const double *evr, const double *evi, int n, int periodic);

/* send the event if it changed since the last one sent; final 0 (during a
   run) sends at most ten a second */
void auto_data_update(const Session &s, int final);

/* on 1 ... on 0 around a redraw of the diagram (redraw_diagram): the
   strip and the circle keep what they show (a run's last point, a grab's
   point) while it plots every point again, auto_data_info and
   auto_data_stab ignored until the last on 0. Nests. */
void auto_data_hold(Session &s, int on);

} // namespace xpp
#endif
