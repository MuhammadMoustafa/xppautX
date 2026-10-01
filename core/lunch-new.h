#ifndef _lunch_new_h_
#define _lunch_new_h_


#include <stdio.h>
#include "form_ode.h"
#include <string>
#include <string_view>
#include <vector>
#include "struct.h" /* GRAPH */
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */
struct Model;   /* model.h */

/* what a set file's readers (io_int, io_double, io_string, io_heading
   reading) throw at a line they cannot read: the line (from 1) and what
   is wrong with it. read_lunch, io_parameter_file and the readers of a
   session's members (xpp_session.cpp) catch it and say so. */
struct SetLineError {
  int line;
  std::string cause;
  /* as an Error of the reader where, at its line of file ("" when the
     caller adds it) */
  Error error(std::string_view where, std::string_view file) const;
};

/* a number of a set file (f READEM: the number its line starts with, the
   name written after it ignored), or z written with its name ss; a line
   missing or not starting with one throws SetLineError */
void io_int(int *i, FILE *fp, int f, std::string_view ss);
void io_double(double *z, FILE *fp, int f, std::string_view ss);
/* one line of a set file into s, whole (f READEM; SetLineError at the
   end of the file), or s written as one */
void io_string(std::string &s, FILE *fp, int f);
/* a "# ..." heading: written (f WRITEM), or on reading a set file of
   xppautX's (one that begins with "## Set file") skipped, a line that is
   not one throwing SetLineError */
void io_heading(int f, FILE *fp, const char *heading);
/* the parameters or the initial conditions of s read from (flag READEM)
   or written to the file fn (-parfile, -icfile, the values panel) */
void io_parameter_file(Session &s, std::string_view fn, int flag);
void io_ic_file(Session &s, std::string_view fn, int flag);
/* the values panel's Save/Load of s's .par and .ic (docs/protocol.md
   "values"), through io_parameter_file/io_ic_file: name empty asks for
   one like Save data does, given skips the ask */
void save_parameter_file(Session &s, std::string name);
void save_ic_file(Session &s, std::string name);
void load_parameter_file(Session &s, std::string name);
void load_ic_file(Session &s, std::string name);
/* the model's internal sets (name, whether -silent runs it, what it
   sets), its parameters' values in the file and its initial conditions,
   those asked for, each under its # heading, into name (-silent's
   -qsets/-qpars/-qics, the protocol's `values` `query`) */
void write_values_query(const Session &s, std::string_view name, bool sets, bool pars, bool ics);

/* The session s's set files (File > Write set, Read set: do_lunch, f 1
   to read and 0 to write), its info file (File > Save info) and their
   parts: the numerics, the delays, BCs, ICs and parameters (io_exprs),
   the active plot window (io_graph); a PostScript picture's parameters
   (ps_write_pars) */
void file_inf(Session &s);
void ps_write_pars(const Session &s, FILE *fp);
void do_info(const Session &s, FILE *fp);
/* s's settings read from the set file fp (File > Read set, -setfile, a
   session's model.set), the front end shown them when redraw; the error
   when it is not one of this model's or one of its lines cannot be read
   (what comes before that line is read) */
Result<> read_lunch(Session &s, FILE *fp, bool redraw);
void write_lunch(Session &s, FILE *fp);
void do_lunch(Session &s, int f);
void dump_eqn(const Session &s, FILE *fp);
void io_numerics(Session &s, int f, FILE *fp);
void io_parameters(Session &s, int f, FILE *fp);
void io_exprs(Session &s, int f, FILE *fp);
void io_graph(Session &s, int f, FILE *fp);

/* A session file's pieces (xpp_session.cpp, W57), in a set file's lines:
   one plot window's settings as io_graph writes the current one's */
void write_graph(FILE *fp, GRAPH &g);
void read_graph(FILE *fp, GRAPH &g);

} // namespace xpp
#endif
