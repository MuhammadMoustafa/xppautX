#pragma once
#include <stddef.h>

#include <string>
#include <vector>
#include <span>
#include "autox_codes.h"

/* autoX's info strip and stability circle as data, for a front end that
   draws them itself: the "autoxinfo" event (docs/protocol.md "The autoX
   diagram as data", docs/ui-v2.md event 7).

   - The info strip (window 103 of the X11 program) is what
     traverse_out() in autox_nox.cpp shows for the point a grab's cursor is on:
     branch, point, type, label, the parameters, the norm, the plotted
     variable's value and the period.
   - The stability circle (window 102) holds a stored diagram point's
     stability values (autox_stability.h): for a steady state e^lambda of
     each eigenvalue lambda (inside the unit circle is stable), for a
     periodic orbit its Floquet multipliers, all zeros where autoX did not
     compute them (a run's first point, unless it restarts from a label of
     the same kind). Every point autoX stores shows it, so after a run it
     is the last point's, while grabbing the cursor's; a redraw of the
     diagram leaves the strip and the circle as they were.

   - Why the run's last branch ended (autox_stop.h, T23): "stop", null
     until a branch ends; a run's start and a new autoX window clear it.

   autox_nox.cpp reports the strip and the circle as it draws them; the front
   end sends the event at the end of a command, before every prompt and at
   most ten times a second while autoX runs, and only when it differs from the one it sent
   last. The client's subscription and pacing are the protocol client's
   (ui_json_internal.h's ProtocolSession).

   autox_data.cpp; nothing escapes it. */

namespace xpp {

struct Session; /* session.h */

/* the event's line now; point: the index of the shown diagram entry in the data the client holds (-1: none); empty: none to send */
std::string autox_data_event_line(const Session &s, const int &point);

/* {"cmd":"data"} with or without "autoxinfo": the event is sent at the next
   update whatever it holds */
void autox_data_subscribe(int on);

/* a new autoX window, or none: nothing to show (no stop reason either) */
void autox_data_forget(Session &s);

/* the point the info strip shows; names are the model's, whole. p2name is
   NULL for a one-parameter point (the strip shows a blank name and 0) */
struct AutoxDataInfo {
    int ibr, pt;     /* autoX's branch and point number, signed as autoX has them */
    AutoxPointCode itp; /* autoX's point type */
    int lab;         /* autoX's label */
    DiagramPointType type; /* stable steady state, unstable, stable periodic, unstable periodic */
    AutoxCalculation flag2;       /* two-parameter curve kind, no_calculation for one parameter */
    int node;        /* DIAGRAM.index of the point */
    int jacobian;    /* DIAGRAM.jacobian: how its run formed the Jacobian */
    const char *sym; /* the point type's name (autox_point_kind_name: regular, fold, hopf, ...) */
    const char *p1name, *p2name;
    double p1, p2;
    double norm;
    const char *vname; /* the variable of the Axes setting and its value (u0) */
    double u;
    double per;
    double x, y, y2; /* where the diagram plots it (autox_xy_plot) */
};

/* what the strip and the circle of a Session show (AutoxState::shown) */
struct AutoxDataShown {
    AutoxDataInfo v{}; /* its names in the strings below */
    std::string sym, p1name, p2name, vname;
    bool two = false;
    bool has_info = false, has_stab = false, stab_periodic = false;
    std::vector<double> stab_re, stab_im;
    int held = 0; /* autox_data_hold depth */
};

void autox_data_info(Session &s, const AutoxDataInfo *info);

/* the stability circle now shows these n values (evr + i evi); periodic:
   Floquet multipliers, else e^lambda of a steady state's eigenvalues */
void autox_data_stab(Session &s, std::span<const double> evr, std::span<const double> evi, int periodic);

/* send the event if it changed since the last one sent; final 0 (during a
   run) sends at most ten a second */
void autox_data_update(const Session &s, int final);

/* on 1 ... on 0 around a redraw of the diagram (redraw_diagram): the
   strip and the circle keep what they show (a run's last point, a grab's
   point) while it plots every point again, autox_data_info and
   autox_data_stab ignored until the last on 0. Nests. */
void autox_data_hold(Session &s, int on);

} // namespace xpp
