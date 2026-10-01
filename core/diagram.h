#ifndef _diagram_h_
#define _diagram_h_
/* AUTO's bifurcation diagram (diagram.cpp). C++ only. */

#include <stdio.h>
#include "auto_nox.h"

#include <deque>
#include <vector>

/* a diagram point and the arrays its DIAGRAM entry points at */
struct DiagramPoint {
  xpp::DIAGRAM d{};
  std::vector<double> uhi,ulo,u0,ubar,evr,evi;
};

/* AUTO's bifurcation diagram, a Session's (session.h): its points in the
   order they were stored, point i with index i (so next/prev are index
   +/- 1). A deque, so a point's address stays valid while points are
   added. */
struct AutoDiagram {
  std::deque<DiagramPoint> points;
};

namespace xpp {
struct Session; /* session.h */

/* A diagram point's type (get_bif_type, autevd.cpp, from the signs AUTO
   gives its branch and point numbers; a run's `ty` in docs/protocol.md's
   `diagram`, `type` in `autoinfo`): a steady state, judged by its
   eigenvalues, or a periodic orbit, by its Floquet multipliers; stable
   or not. The events carry stable and periodic flags beside it. */
enum DiagramPointType : int { STABLE_EQ = 1, UNSTABLE_EQ = 2, STABLE_PERIODIC = 3, UNSTABLE_PERIODIC = 4 };
constexpr bool point_is_stable(int type) { return type == STABLE_EQ || type == STABLE_PERIODIC; }
constexpr bool point_is_periodic(int type) { return type == STABLE_PERIODIC || type == UNSTABLE_PERIODIC; }
}

/* The session s's diagram: a list of points in the order they were
   stored. start_diagram empties it to one point of n variables, which the
   run's first point (edit_start) fills in place; add_diagram appends.
   Point i has index i. */
void start_diagram(xpp::Session &s, int n);
/* how many points (1 for an empty diagram, 0 before start_diagram) */
int diagram_count(const AutoDiagram &diagram);
/* the point of that index, or NULL */
xpp::DIAGRAM *diagram_point(AutoDiagram &diagram, int index);
/* the first point, or NULL before start_diagram */
xpp::DIAGRAM *diagram_first(AutoDiagram &diagram);
/* the point after / before d, or NULL at the end / the start */
xpp::DIAGRAM *diagram_next(AutoDiagram &diagram, const xpp::DIAGRAM *d);
xpp::DIAGRAM *diagram_prev(AutoDiagram &diagram, const xpp::DIAGRAM *d);
void edit_start(xpp::Session &s, int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3, int icp4,double *evr, double *evi);
void add_diagram(xpp::Session &s, int ibr, int ntot, int itp, int lab, int nfpar, double a, double *uhi, double *ulo, double *u0, double *ubar, double *par, double per, int n, int icp1, int icp2, int icp3,int icp4,int flag2, double *evr, double *evi);
void kill_diagrams(xpp::Session &s);
/* the entry add_diagram or edit_start made last */
xpp::DIAGRAM *last_diagram(AutoDiagram &diagram);
/* the stored point labelled lab (the first, as AUTO reads its .s file), or NULL */
const xpp::DIAGRAM *diagram_of_label(const xpp::Session &s, int lab);
/* entry `index` of the diagram list is AUTO's point ntot of branch ibr (either sign of ntot) */
int diagram_has(const AutoDiagram &diagram, int index, int ibr, int ntot);
void redraw_diagram(xpp::Session &s);
void write_info_out(xpp::Session &s);
void write_init_data_file(xpp::Session &s);
void write_pts(xpp::Session &s);
/* the points of branch ibr from pts to pte into the data browser */
void load_browser_with_branch(xpp::Session &s, int ibr, int pts, int pte);
/* the diagram as a picture, in image_format.h's IMAGE_FORMAT_PS or
   IMAGE_FORMAT_SVG (W53, issue #101; GIF has no vector picture to draw
   here) */
void export_auto_picture(xpp::Session &s, int fmt);
void bound_diagram(xpp::Session &s, double *xlo, double *xhi, double *ylo, double *yhi);
/* the diagram of an XPPAUT .auto file, at fp after its settings (AUTO's
   File/Load diagram imports one: auto_nox.cpp import_auto_file): 1 read,
   -1 an empty diagram */
int load_diagram(xpp::Session &s, FILE *fp, int node);

/* points, whole (an .autox's diagram, autox.h), in place of the session
   s's diagram: each point's DIAGRAM arrays and index set to its own, the
   first point filled in (DiagFlag); none is start_diagram's empty diagram */
void diagram_restore(xpp::Session &s, std::deque<DiagramPoint> points);
#endif
