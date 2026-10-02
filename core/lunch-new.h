#ifndef _lunch_new_h_
#define _lunch_new_h_


#include <stdio.h>
#include "form_ode.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "adj2.h"      /* AdjointState's transpose */
#include "arrayplot.h" /* APLOT */
#include "integrate.h" /* RangeVars, EquilibriumRange */
#include "pp_shoot.h"  /* ShootRange */
#include "struct.h"    /* GRAPH */
#include "xpp_error.h"
#include "xpp_io.h"    /* Lines */

namespace xpp {

struct Session; /* session.h */
struct Model;   /* model.h */

/* ---- The set format: a session's model.set, windows.set and marks.set,
   a parameter file (.par) and XPPAUT's set file (an import) are lines of it ----
   Written here; read through xpp_io.h's Lines (whole(), real(), next(),
   heading()), the one way a file of ours is read (W125). */

/* "value   name" (a whole number), "value  name" (a number, %.16g), a
   line of text whole, a "# ..." heading */
void write_whole(FILE *fp, int value, std::string_view name);
void write_real(FILE *fp, double value, std::string_view name);
void write_text(FILE *fp, std::string_view text);
void write_heading(FILE *fp, std::string_view heading);

/* one plot window's settings as a set file holds the active one's
   (# Graphics); read into g, whose other fields stay, after its heading */
void write_graph(FILE *fp, const GRAPH &g);
void read_graph(Lines &lines, GRAPH &g);
/* those settings of from given to to, its other fields kept (a restored
   window, as made, takes the saved one's) */
void copy_graph_settings(const GRAPH &from, GRAPH &to);

/* What a set file holds, read whole (read_session_set, import_xppaut_set)
   before anything is applied (apply_set_file): the numerics, the delays,
   the boundary conditions, the initial conditions and parameters, the
   active window's graphics, Transpose, the H functions' coupling, the
   array plot, the torus and the ranges. */
struct SetFile {
  int njmp = 0, nmesh = 0, method = 0;
  double tend = 0, delta_t = 0, t0 = 0, trans = 0, bound = 0, hmin = 0, hmax = 0, toler = 0, atoler = 0, delay = 0;
  int evec_iter = 0;
  double evec_err = 0, newt_err = 0, poipln = 0, bvp_tol = 0, bvp_eps = 0;
  int bvp_maxit = 0, poimap = 0, poivar = 0, poisgn = 0, sos = 0, delay_flag = 0;
  double current_time = 0, last_time = 0;
  int my_start = 0, inflag = 0;
  /* Max points for volterra, when the method is Volterra's */
  std::optional<int> volterra_points;
  std::vector<std::string> delays, bcs;
  std::vector<double> last_ic, current, params;
  GRAPH graph{};
  /* xppautX's only (ours) */
  decltype(AdjointState::transpose) transpose;
  std::vector<std::string> coupling;
  APLOT aplot{};
  int torus = 0;
  double tor_period = 0;
  std::vector<int> itor;
  EquilibriumRange eq_range;
  RangeVars range{};
  ShootRange shoot_range;
};

/* text, a session's set file named file, read for the session s (its
   model; what the file does not hold, the active window's other
   settings, are s's): every line checked, or the error at the line that
   is wrong (not one of this model's, a value that does not read or is out
   of range, a line after the last value) */
Result<SetFile> read_session_set(const Session &s, std::string file, std::string_view text);
/* f applied to s, in one step (the front end shown it when redraw) */
void apply_set_file(Session &s, const SetFile &f, bool redraw);
/* XPPAUT's set file at path read and applied (File > Import XPPAUT set,
   -setfile), or the error, nothing applied: its model's equations follow
   its last value ("RHS etc ...", not read), and a file without them (a
   session's model.set, a file of ours) is refused at its end. Every named
   value must have its expected name. Valid values are applied, then saved
   through the session writer to <base>.snapx beside path, which becomes
   the session open; one message names it. A save error is returned with
   the imported values still applied. */
Result<> import_xppaut_set(Session &s, std::string_view path, bool redraw);
/* s's settings as a set file: a session's model.set, no equations after it */
void write_lunch(Session &s, FILE *fp);

/* a parameter file's values (the model m's parameters, in order) or an
   initial-conditions file's (one per differential equation), read whole
   from path; the error at the line that is wrong */
Result<std::vector<double>> read_parameter_file(const Model &m, std::string_view path);
Result<std::vector<double>> read_ic_file(const Model &m, std::string_view path);
/* the parameters or the initial conditions of s read whole from the file
   fn and applied, or the error shown and nothing applied (-parfile,
   -icfile, Initialconds/File, the values panel's Load); written to it */
void load_parameter_file_named(Session &s, std::string_view fn);
void load_ic_file_named(Session &s, std::string_view fn);
void write_parameter_file(Session &s, std::string_view fn);
void write_ic_file(const Session &s, std::string_view fn);
/* the values panel's Save/Load of s's .par and .ic (docs/protocol.md
   "values"): name empty asks for one like Save data does, given skips
   the ask */
void save_parameter_file(Session &s, std::string name);
void save_ic_file(Session &s, std::string name);
void load_parameter_file(Session &s, std::string name);
void load_ic_file(Session &s, std::string name);
/* the model's internal sets (name, whether -silent runs it, what it
   sets), its parameters' values in the file and its initial conditions,
   those asked for, each under its # heading, into name (-silent's
   -qsets/-qpars/-qics, the protocol's `values` `query`) */
void write_values_query(const Session &s, std::string_view name, bool sets, bool pars, bool ics);

/* File > Import XPPAUT set (asks for the file), the session s's info file
   (File > Save info); a PostScript picture's parameters (ps_write_pars) */
void import_xppaut_set_command(Session &s);
void file_inf(Session &s);
void ps_write_pars(const Session &s, FILE *fp);
void do_info(const Session &s, FILE *fp);

} // namespace xpp
#endif
