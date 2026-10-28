#pragma once
/* AUTO's bifurcation diagram (diagram.cpp). C++ only. */

#include <stdio.h>
#include "auto_nox.h"
#include "auto_codes.h" /* DiagramPointType */

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_error.h"

/* AUTO's bifurcation diagram, a Session's (session.h): its points in the
   order they were stored, point i with index i, read by index. A deque,
   so a point's address stays valid while points are added. */
struct AutoDiagram {
  std::deque<xpp::DIAGRAM> points;
};

namespace xpp { struct Session; /* session.h */ }

/* The session s's diagram: a list of points in the order they were
   stored. start_diagram empties it to one point of n variables, which the
   run's first point (edit_start) fills in place; add_diagram appends.
   Point i has index i. */
void start_diagram(xpp::Session &s, int n);
/* how many points: start_diagram's empty diagram has one (the run's first
   point, not filled in yet), a diagram before start_diagram none */
int diagram_count(const AutoDiagram &diagram);
/* p, a point of AUTO's run or of a file being read (its arrays n long),
   into the diagram: edit_start fills the first point in place with the
   run's flag2 (two_param), add_diagram appends it with p's; the kind of
   calculation and the torus period are the run's (s) */
void edit_start(xpp::Session &s, const xpp::DIAGRAM &p);
void add_diagram(xpp::Session &s, const xpp::DIAGRAM &p);
/* the entry add_diagram or edit_start made last, or NULL before start_diagram */
xpp::DIAGRAM *last_diagram(AutoDiagram &diagram);
/* the stored point labelled lab (the first, as AUTO reads its .s file), or NULL */
const xpp::DIAGRAM *diagram_of_label(const xpp::Session &s, int lab);
/* entry `index` of the diagram is AUTO's point ntot of branch ibr (either sign of ntot) */
bool diagram_has(const AutoDiagram &diagram, int index, int ibr, int ntot);
/* the first point of branch ibr numbered ntot (either sign: the sign is
   its stability), or NULL */
const xpp::DIAGRAM *diagram_find(const AutoDiagram &diagram, int ibr, int ntot);
void redraw_diagram(xpp::Session &s);
/* The diagram's text exports (File's All info, init Data and Write pts), to `file` (a name in the model's folder), or
   asked for with the dialog's default name when none is given; nothing is written for a declined question. The
   default names, what the dialog suggests, are diagram_default_file's. */
xpp::Result<> write_info_out(xpp::Session &s, const std::optional<std::string> &file);
xpp::Result<> write_init_data_file(xpp::Session &s, const std::optional<std::string> &file);
xpp::Result<> write_pts(xpp::Session &s, const std::optional<std::string> &file);
/* the name the dialog of export `what` ("allinfo", "initdata", "diagram") suggests, with extension (".dat") */
std::string diagram_default_file(const xpp::Session &s, std::string_view what, std::string_view extension);
/* the points of branch ibr from pts to pte into the data browser */
void load_browser_with_branch(xpp::Session &s, int ibr, int pts, int pte);
/* the diagram as a picture, in image_format.h's IMAGE_FORMAT_PS or
   IMAGE_FORMAT_SVG (W53, issue #101; GIF has no vector picture to draw
   here) */
xpp::Result<> export_auto_picture(xpp::Session &s, int fmt, const std::optional<std::string> &file);
void bound_diagram(xpp::Session &s, double *xlo, double *xhi, double *ylo, double *yhi);
/* points, whole (a .snapx's diagram, xpp_session_auto.h), in place of the session
   s's diagram: each point's index set, the
   first point filled in (DiagFlag); none is start_diagram's empty diagram */
void diagram_restore(xpp::Session &s, std::deque<xpp::DIAGRAM> points);
/* Resolve a model parameter (its Model index) of a point from its immutable branch constants and continued values. */
double diagram_parameter(const xpp::DIAGRAM &point, int parameter);
/* What a point's axis is worth: the model parameter's value, the point's period for the period, 0 for another
   AUTO unknown (no model value) */
double diagram_axis_value(const xpp::DIAGRAM &point, xpp::AutoAxis axis);
std::vector<double> diagram_parameters(const xpp::DIAGRAM &point);
/* Attach the current computed branch, capturing all model parameters once. */
void diagram_record_branch(xpp::Session &s, xpp::DIAGRAM &point);
