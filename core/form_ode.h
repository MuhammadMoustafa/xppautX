#ifndef _form_ode_h
#define _form_ode_h

#include "xpplim.h"
#include "newpars.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MAXLINES 5000

/* the columns a batch run writes (plotlist, N_plist of them): an "only"
   statement's (create_plot_list), or post-processing's; what the model
   itself defines is xpp::Model's (model.h) */
extern int *plotlist,N_plist;
/* an old-style file being rewritten in the new syntax (-convert):
   ConvertStyle set, convertf the new file (markov.cpp writes it too) */
extern int ConvertStyle;
extern FILE *convertf;

int make_eqn(void);
void strip_saveqn(void);
void create_plot_list(void);
/* a model statement that calls compiled C code (export, dll_lib, dll_fun,
   a network's import; W55 removed them): logs why the line is refused
   and returns -1, the parsers' "this line does not parse" */
int refuse_compiled_functions(const char *what);
/* the initial values an .odex model gives as formulas
   (Model::initial_values), each evaluated with every parameter set:
   set_all_vals (load_eqn.cpp) calls this once the model is set up, where
   an .ode's array initial values are evaluated too */
void set_initial_values(void);


#ifdef __cplusplus
}

#include "odex.h"

#include <functional>
#include <string>
#include <string_view>

/* The Model builder: a model's statements (odex.h), whichever reader made
   them, into the current Model and Session; a statement that does not
   build fails the load (xpp_model_failed, at the statement: the load's
   diagnostic). In an .odex model (Parsed::derived), a fixed quantity that
   reads only parameters, consts and pure functions becomes a derived one
   (docs/odex.md question 9); an .ode's stays fixed, as XPPAUT's. */
void build_model(xpp::odex::Parsed p);
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
void build_old_style(int neq, FILE *fptr, const std::function<bool(std::string &)> &next_line);
/* 1 when the model is a map: is_a_map, or file ends in .dis or .dif */
int disc(std::string_view file);
/* boundary condition i's formula (Model::bcs[i].string, at most 255 bytes
   of it) becomes string; set_bc makes the condition first */
void set_bc_formula(int i, std::string_view string);
/* formula i (Model::formulas) becomes text */
void set_ode_name(int i, std::string_view text);
#endif
#endif
