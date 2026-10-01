#ifndef _form_ode_h
#define _form_ode_h

#include "xpplim.h"
#include "newpars.h"
#include "odex.h"
#include <stdio.h>

#include <functional>
#include <string>
#include <string_view>

#define MAXLINES 5000

namespace xpp {

struct Model;   /* model.h */
struct Session; /* session.h */

/* -convert: an old-style file read is rewritten in the new syntax, into
   the loading Session's parser.convert (markov.cpp writes it too). The
   command line's flag (comline.cpp), which a model read in the new
   syntax clears for the rest of the run */
extern int ConvertStyle;

/* a model statement that calls compiled C code (export, dll_lib, dll_fun,
   a network's import; W55 removed them): logs why the line is refused
   and returns -1, the parsers' "this line does not parse" */
int refuse_compiled_functions(std::string_view what);

/* The load's readers and builder fill the Model and Session the load
   builds (xpp::Load, W47d3), which they are given. */

/* a model typed in at the terminal (no file given): read into s's Model */
int make_eqn(Session &s);
/* the model's source lines without control characters */
void strip_saveqn(Model &m);
/* the columns a batch run writes: the model's "only" statement's */
/* the columns a batch run writes (Session::plot_list): the model's
   "only" statement's */
void create_plot_list(Session &s);
/* the initial values an .odex model gives as formulas
   (Model::initial_values), each evaluated with every parameter set:
   set_all_vals (load_eqn.cpp) calls this once the model is set up, where
   an .ode's array initial values are evaluated too */
void set_initial_values(Session &s);
/* The Model builder: a model's statements (odex.h), whichever reader made
   them, into the Model and Session s; a statement that does not
   build fails the load (model_failed, at the statement: the load's
   diagnostic). In an .odex model (Parsed::derived), a fixed quantity that
   reads only parameters, consts and pure functions becomes a derived one
   (docs/odex.md question 9); an .ode's stays fixed, as XPPAUT's. */
void build_model(Session &s, odex::Parsed p);
/* While one lives (xppautX --convert's check, odex_convert.cpp), an .ode
   model is built with .odex's derived quantities, so that the .ode and
   the .odex written from it compile alike (the numbers are the same
   either way); otherwise an .ode's fixed quantities stay fixed. */
class OdeAsOdex {
public:
  OdeAsOdex();
  ~OdeAsOdex();
  OdeAsOdex(const OdeAsOdex &) = delete;
  OdeAsOdex &operator=(const OdeAsOdex &) = delete;
};
/* an old-style model, neq its number of equations: each line next_line
   gives (false at the end) built as it is read, fptr the file (a Markov
   table's lines are read from it) */
void build_old_style(Session &s, int neq, FILE *fptr, const std::function<bool(std::string &)> &next_line);
/* 1 when the model is a map: is_a_map, or file ends in .dis or .dif */
int disc(const Model &m);
/* the Session s's boundary condition i's formula (Session::bcs[i].string,
   at most 255 bytes of it) becomes string */
void set_bc_formula(Session &s, int i, std::string_view string);
/* formula i (Model::formulas) becomes text */
void set_ode_name(Model &m, int i, std::string_view text);

} // namespace xpp
#endif
