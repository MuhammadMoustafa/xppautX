#ifndef XPP_MODEL_H
#define XPP_MODEL_H
/* xpp::Model: what loading a model produces (CLAUDE.md "No global state";
   docs/roadmap.md W46c, the start of W47b). C++ only.

   For now the current Model is reached through xpp::model(); W47d passes
   it explicitly. A load (xpp_batch.cpp's xpp_load_model) builds a new one
   through ModelLoad below and keeps it only when the load succeeds.

   A Model is what the .ode file defines, and a load is the only thing
   that should write it; what a person changes while working (parameter
   values, initial conditions, the numerics settings) is the Session's
   (W47c), which the Model gives its defaults (default_val, default_ic,
   options). Two writers remain, both W47c's to move: browse_data's added
   column (a formula column appended as one more variable: neq, nvar,
   uvar_names, its program) and the runtime compiler's scratch symbols
   (ncon/nsym grow past ncon_start/nsym_start and roll back).

   The name tables hold a model's names as the parser keeps them (blanks
   removed, upper case), each at most XPP_NAME_MAX long: the parser refuses
   a longer one (name_too_long), so nothing here cuts a name. A display of
   fixed width shortens one with short_name (xpp_util.h). The tables have
   the parser's fixed limits as their sizes (xpplim.h): the parser writes
   an entry by its index (a variable, a Markov variable, an aux quantity
   each at its own offset), and an index past what the model uses reads
   an empty name. */
#include "xpplim.h"
#include "volterra.h"

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
  /* each variable's kind: 1 a Volterra integral equation (x(t)=...),
     0 an ODE or a map */
  std::array<int,MAXODE> eq_type{};

  /* ---- the compiled right-hand sides (form_ode.cpp) ---- */
  /* each quantity's formula as typed and as compiled for evaluate(), by
     the index form_ode.cpp's compile_em gives it: the ODEs (node), the
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
  /* the boundary conditions, one per ODE */
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

  /* ---- the user functions (parserslow2.cpp), nfun of them ---- */
  int nfun=0;
  /* each one's argument count */
  std::array<int,MAXUFUN> narg_fun{};
  /* each one's formula as typed */
  std::array<std::string,MAXUFUN> ufun_defs;
  /* each one compiled: MAXEXPLEN commands, the formula then ENDFUN,
     its argument count and ENDEXP */
  std::array<std::vector<int>,MAXUFUN> ufun_programs;
  /* the loaded file's path, as given ("console" for standard input) */
  std::string this_file;
  /* "_<name>" of the internal set last applied, "" when none */
  std::string this_internset;
};

namespace detail {
/* the current Model's slot: constant-initialised (no guard on a read),
   filled on first use and changed only by ModelLoad */
inline Model *&current_model() noexcept
{
  static constinit Model *current=nullptr;
  return current;
}
Model *first_model();
}

/* the current Model. Inline: the integrator's right-hand side reads the
   counts on every step, and a call per read slowed it measurably. */
inline Model &model()
{
  Model *m=detail::current_model();
  if(!m)[[unlikely]]m=detail::first_model();
  return *m;
}

/* A load in progress: while it lives the current Model is a fresh one,
   which the parser fills; commit() keeps it and drops the one before,
   and a load that never commits (it failed) puts the one before back.
   Model memory is never freed while a pointer into it may be held: the
   old one goes at commit, when the new model has replaced every use. */
class ModelLoad {
public:
  ModelLoad();
  ~ModelLoad();
  ModelLoad(const ModelLoad &)=delete;
  ModelLoad &operator=(const ModelLoad &)=delete;
  void commit();
private:
  Model *previous;
  bool committed=false;
};

}

#endif
