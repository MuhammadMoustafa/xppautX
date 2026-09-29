#ifndef XPP_MODEL_H
#define XPP_MODEL_H
/* xpp::Model: what loading a model produces (CLAUDE.md "No global state";
   docs/roadmap.md W46c, the start of W47b). C++ only.

   For now the current Model is reached through xpp::model(); W47d passes
   it explicitly. A load (xpp_batch.cpp's xpp_load_model) builds a new one
   with a new Session through xpp::Load (session.h) and keeps them only
   when the load succeeds.

   A Model is what the .ode file defines, and a load is the only thing
   that should write it; what a person changes while working (parameter
   values, initial conditions, the numerics settings) is the Session's
   (W47c), which the Model gives its defaults (default_val, default_ic,
   options, bcs). What still writes it after a load is the browser's
   added column (one more variable: neq, nvar, uvar_names, formulas,
   programs), which W47c left: it touches every reader of neq. The
   tables, whose values are computed or read again while working, are
   the Session's.

   The name tables hold a model's names as the parser keeps them (blanks
   removed, upper case), each of any length (W76): nothing here cuts a
   name. A display of fixed width shortens one with short_name
   (xpp_util.h). The tables have
   the parser's fixed limits as their sizes (xpplim.h): the parser writes
   an entry by its index (a variable, a Markov variable, an aux quantity
   each at its own offset), and an index past what the model uses reads
   an empty name. */
#include "xpplim.h"
#include "diagnostic.h"
#include "volterra.h"
#include "tabular.h"
#include "simplenet.h"
#include "xpp_current.h"
#include "odex.h"

#include <array>
#include <string>
#include <vector>

namespace xpp {

struct Model {
  /* ---- counts and kinds (form_ode.cpp's parser sets them) ---- */
  /* the stored columns but time: the ODEs, the Markov variables and the
     aux quantities (browse_data's added column takes one more) */
  int neq=0;
  /* the ODEs (the fixed variables too while form_ode reads the file) */
  int node=0;
  /* the Markov variables, after the ODEs */
  int nmarkov=0;
  /* the fixed variables, after the ODEs and Markov variables */
  int fix_var=0;
  /* the parameters */
  int nupar=0;
  /* the parser's variables (t, the model's variables, then its extras) */
  int nvar=0;
  /* the Wiener parameters */
  int nwiener=0;
  /* the first constant and symbol after the model's own: an expression
     compiled later (a browser column, a histogram's condition) adds its
     own above them and rolls back to them */
  int ncon_start=0,nsym_start=0;
  /* the first primed symbol (a variable's x') */
  int prime_start=0;
  /* the model's formulas divide as IEEE does (1/0 is inf, 0/0 NaN), where
     an .ode's replace a zero divisor by ZERO_DIVISOR (expr_compile.cpp;
     docs/odex.md, question 1): the builder's copy of its statement list's
     Parsed::ieee_division, which only the .odex reader sets */
  bool ieee_division=false;
  /* each variable's kind: 1 a Volterra integral equation (x(t)=...),
     0 an ODE or a map */
  std::array<int,MAXODE> eq_type{};

  /* ---- the compiled right-hand sides (form_ode.cpp) ---- */
  /* each quantity's formula as typed and as compiled for evaluate(), by
     the index form_ode.cpp's builder gives it: the ODEs (node), the
     fixed variables (fix_var), the Markov variables' transition
     placeholders, then the aux quantities; browse_data's added column
     takes the next one. An unused index has "" and no program. */
  std::array<std::string,MAXODE> formulas;
  std::array<std::vector<int>,MAXODE> programs;
  /* a boundary condition, 0=string: string (at most 255 bytes, the rest
     NUL), com its compiled form (200 commands), name "0=" (10 bytes;
     pp_shoot writes its side into it). C buffers: the shooting code and
     the dialogs write into them. */
  struct BoundaryCondition {
    std::vector<int> com;
    std::vector<char> string;
    std::vector<char> name;
  };
  /* the boundary conditions, one per ODE, as the model gives them (the
     ones in use are the Session's) */
  std::array<BoundaryCondition,MAXODE> bcs;

  /* a fixed variable's name and formula as typed (lunch-new.cpp writes
     them), fix_var of them */
  struct FixedVariable {
    std::string name,value;
  };
  std::array<FixedVariable,MAXODE> fixinfo;

  /* ---- the global flags (flags.cpp), nflags of them ---- */
  /* a flag: when the condition's value (cond, compiled comcond) crosses
     0 in the direction sign (1 up, -1 down, 0 at 0), set each of its
     nevents events' lhs to its formula (rhs, compiled comrhs); type 0 a
     variable, 1 a parameter (anypars when any is), 2 "out_put", 3
     "arret"; nointerp for "no_interp" */
  static constexpr int max_events=20;
  struct GlobalFlag {
    std::array<int,max_events> lhs{};
    std::array<std::string,max_events> lhsname;
    std::array<std::string,max_events> rhs;
    std::array<std::vector<int>,max_events> comrhs;
    std::string cond;
    std::vector<int> comcond;
    int sign=0,nevents=0;
    std::array<int,max_events> type{};
    int anypars=0;
    int nointerp=0;
  };
  int nflags=0;
  std::array<GlobalFlag,MAXFLAG> flags;

  /* ---- the DAEs (dae_fun.cpp) ---- */
  /* an algebraic variable (solv): its name, the formula of its first
     guess (rhs, compiled form), its place in the parser's variables */
  struct AlgebraicVariable {
    std::string name,rhs;
    std::vector<int> form;
    int index=0;
  };
  /* an algebraic condition 0=rhs (compiled form) */
  struct AlgebraicEquation {
    std::string rhs;
    std::vector<int> form;
  };
  int nsvar=0,naeqn=0;
  std::array<AlgebraicVariable,MAXDAE> svars;
  std::array<AlgebraicEquation,MAXDAE> aeqns;

  /* ---- the model's own values and options ---- */
  /* the parameters' values and the variables' initial conditions as the
     model gives them (the ones in use are the Session's: constants,
     last_ic) */
  std::array<double,MAXPAR> default_val{};
  std::array<double,MAXODE> default_ic{};
  /* the initial values a model gives as formulas (an .odex's init): each
     variable (by name) and its formula (the expression engine's text),
     evaluated in order once the model is set up (form_ode.cpp's
     set_initial_values); where its init is, for an error (no cause) */
  struct InitialValue {
    std::string name, formula;
    Diagnostic where;
  };
  std::vector<InitialValue> initial_values;
  /* its @ lines, each whole, as read: load_eqn.cpp's set_internopts
     sets the numerics and plot settings from them (their current values
     are the Session's) */
  std::vector<std::string> options;

  /* ---- delays, integral equations, Markov chains, networks ---- */
  /* the delay terms the parser compiled (delay(), a delayed network) */
  int ndelays=0;
  /* the integral equations' kernels (nkernel of them) */
  int nkernel=0;
  std::array<KERNEL,MAXKER> kernels;

  /* a Markov variable's chain: nstates states and their values, and the
     nstates x nstates transitions as typed (trans) and compiled
     (command), or as numbers (fixed) when type is 1 (fixed for all time;
     0 when they depend on the state) */
  struct MarkovChain {
    std::vector<std::vector<int>> command;
    std::vector<std::string> trans;
    std::vector<double> fixed;
    int nstates=0;
    std::vector<double> states;
    int type=0;
    std::string name;
  };
  /* the Markov variables' chains (nmarkov of them) */
  std::array<MarkovChain,MAXMARK> markov;
  /* the Wiener parameters' places in the parser's constants (nwiener) */
  std::array<int,MAXPAR> wiener{};

  /* the variables' names by index: the ODEs (NODE), the Markov variables
     (NMarkov), then the aux quantities, NEQ in all; browse_data's added
     column takes the next one */
  std::array<std::string, MAXODE> uvar_names;
  /* the parameters' names (NUPAR of them) */
  std::array<std::string, MAXPAR> upar_names;
  /* the user functions' names (NFUN of them) */
  std::array<std::string, MAXUFUN> ufun_names;
  /* each user function's argument names (narg_fun[i] of them) */
  std::array<std::vector<std::string>, MAXUFUN> ufun_args;

  /* ---- the networks and vectorizers (simplenet.cpp) ---- */
  int nnetwork=0,nvector=0;
  std::array<Network,MAXNET> networks;
  std::array<Vectorizer,MAXVEC> vectors;

  /* ---- the user functions (expr_symbols.cpp), nfun of them ---- */
  int nfun=0;
  /* each one's argument count */
  std::array<int,MAXUFUN> narg_fun{};
  /* each one's formula as typed */
  std::array<std::string,MAXUFUN> ufun_defs;
  /* each one compiled: MAXEXPLEN commands, the formula then ENDFUN,
     its argument count and ENDEXP */
  std::array<std::vector<int>,MAXUFUN> ufun_programs;
  /* ---- the source ---- */
  /* the model file's lines as read (strip_saveqn makes their control
     characters blanks at the end of the load) */
  std::vector<std::string> source;
  int nlines() const { return static_cast<int>(source.size()); }
  /* its statements as its reader read them (odex.h; an .ode's arrays
     expanded), in order: xppautX --convert writes them (odex_convert.cpp) */
  std::vector<odex::Statement> statements;
  /* a " comment of the model: its text, and with {name=value,...} an
     action, "$ name=value ..." (aflag 1), run when it is picked */
  struct Comment {
    std::string text,action;
    int aflag=0;
  };
  std::vector<Comment> comments;
  /* the names an "only" statement keeps in a batch run's output */
  std::vector<std::string> only;
  /* a named set of options (@ set name {...}): does holds them as
     "$ name=value ..." */
  struct InternalSet {
    std::string name,does;
  };
  std::vector<InternalSet> intern_sets;
  /* the options file (the model's "options" line, else default.opt) that
     set_all_vals reads */
  std::string options_file;
  /* the loaded file's path, as given ("console" for standard input) */
  std::string this_file;
};

/* the current Model (xpp_current.h) */
inline Model &model()
{
  return detail::current<Model>();
}


}

#endif
