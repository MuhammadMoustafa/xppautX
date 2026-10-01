#ifndef _lunch_new_h_
#define _lunch_new_h_


#include <stdio.h>
#include "form_ode.h"
#include <string>
#include <string_view>
#include <vector>
#include "struct.h" /* GRAPH */

namespace xpp {

struct Session; /* session.h */
struct Model;   /* model.h */

/* a number of a set file (f READEM), or z written with its name ss */
void io_int(int *i, FILE *fp, int f, std::string_view ss);
void io_double(double *z, FILE *fp, int f, std::string_view ss);
/* one line of a set file into s, whole (f READEM), or s written as one */
void io_string(std::string &s, FILE *fp, int f);
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
int read_lunch(Session &s, FILE *fp);
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
