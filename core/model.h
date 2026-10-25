#pragma once
/* xpp::Model: what loading a model produces (AGENTS.md "No global state";
   docs/roadmap.md W46c, the start of W47b). C++ only.

   A Model is passed to what reads it (a const Model&), or reached through
   the Session that runs it (Session::model(), session.h); there is no
   current one to read (W47d). A load (xpp_batch.cpp's xpp::load_model)
   builds a new one with a new Session through xpp::Load (session.h) and
   keeps them only when the load succeeds.

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
   (xpp_util.h). The per-variable tables are vectors the builder sizes
   with size_variables as the file declares its variables (W243: no
   limit on their number): it writes an entry by its index (a variable, a
   Markov variable, an aux quantity each at its own offset). */
#include "xpplim.h"
#include "xpp_error.h"
#include "volterra.h"
#include "tabular.h"
#include "simplenet.h"
#include "odex.h"
#include "model_files.h"
#include "xpp_tcc.h"
#include "expr_program.h"

#include <array>
#include <memory>
#include <string>
#include <map>
#include <unordered_map>
#include <optional>
#include <set>
#include <vector>

namespace xpp {
struct Session;

class TempDir; /* xpp_files.h */

/* the array initial values: each one's variable x and range j1..j2 and
   its formula (type 2 when in use), at most NAR_IC of them; defined once
   the model gave one. The model's (Model::array_ics), and the run's own
   copy (IntegratorState), which Initialconds edits. */
constexpr int NAR_IC = 50;
struct ArrayIc {
  int index0 = -1, type = 0;
  std::string formula;
  int n = 0;
  std::string var;
  int j1 = 0, j2 = 0;
};
struct ArrayIcs {
  std::array<ArrayIc, NAR_IC> ics{};
  int defined = 0;
};


struct Model {
  /* Room for n parameters in default_val, upar_names and upar_con (it
     only grows; the parser's constants grow in add_con). */
  void size_parameters(int n)
  {
    const std::size_t k=static_cast<std::size_t>(n);
    if(upar_names.size()>=k)return;
    default_val.resize(k);
    upar_names.resize(k);
    upar_con.resize(k);
  }

  /* Room for n variables, fixed variables and aux quantities in the
     per-variable tables below (eq_type, formulas, programs, bcs, fixinfo,
     default_ic, uvar_names): the builder calls it with what the file has
     declared so far, so a table is never larger than the input that made
     it. It only grows. An index below n reads an empty entry. */
  void size_variables(int n)
  {
    const std::size_t k=static_cast<std::size_t>(n);
    if(uvar_names.size()>=k)return;
    eq_type.resize(k);
    formulas.resize(k);
    programs.resize(k);
    bcs.resize(k);
    fixinfo.resize(k);
    default_ic.resize(k);
    uvar_names.resize(k);
  }

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
  std::vector<int> eq_type;

  /* ---- the compiled right-hand sides (form_ode.cpp) ---- */
  /* each quantity's formula as typed and as compiled for evaluate(), by
     the index form_ode.cpp's builder gives it: the ODEs (node), the
     fixed variables (fix_var), the Markov variables' transition
     placeholders, then the aux quantities; browse_data's added column
     takes the next one. An unused index has "" and no program. */
  std::vector<std::string> formulas;
  std::vector<Program> programs;
  /* W258: executable code has exactly this Model's lifetime. */
  std::unique_ptr<tcc::Program> native_program;
  /* compile_model: the right-hand side in its order as one function: the
     fixed variables' values into the variables v (W468), then, given ydot
     and when native_equations, the equations' into ydot (W469); none when
     a fixed variable is interpreted, or when there is none and
     native_equations is false */
  void (*native_rhs)(double *c, double *v, Session *S, double *ydot)=nullptr;
  /* native_rhs writes the equations: each is native, and no network or DAE
     runs between them and the fixed variables */
  bool native_equations=false;
  bool auto_rhs_pure=false; /* compile_model: every AUTO RHS program is native and Session-independent */
  /* the exact Jacobian compile_model made, or why there is none (W260) */
  ExactJacobian jacobian;
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
  std::vector<BoundaryCondition> bcs;
  /* how many of them the model itself wrote (b / bndry / boundary lines); the
     rest are the default 0, and with none the model solves no boundary value
     problem: the state event then sends no bcs */
  int bc_defined=0;

  /* a fixed variable's name and formula as typed (lunch-new.cpp writes
     them), fix_var of them */
  struct FixedVariable {
    std::string name,value;
  };
  std::vector<FixedVariable> fixinfo;

  /* ---- the global flags (flags.cpp) ---- */
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
    std::array<Program,max_events> comrhs;
    std::string cond;
    Program comcond;
    int sign=0,nevents=0;
    std::array<int,max_events> type{};
    int anypars=0;
    int nointerp=0;
  };
  std::vector<GlobalFlag> flags;
  int flag_count() const { return static_cast<int>(flags.size()); }

  /* ---- the derived quantities (derived.cpp): each one's constant (its
     place in the parser's constants) and formula, as text and compiled,
     worked out in this order whenever a parameter changes ---- */
  struct DerivedQuantity {
    int index=0;
    std::string rhs;
    Program form;
    Place where; /* its line, for an error */
  };
  std::vector<DerivedQuantity> derived;

  /* ---- the DAEs (dae_fun.cpp) ---- */
  /* an algebraic variable (solv): its name, the formula of its first
     guess (rhs, compiled form), its place in the parser's variables */
  struct AlgebraicVariable {
    std::string name,rhs;
    Program form;
    int index=0;
    Place where; /* its solv line, for an error */
  };
  /* an algebraic condition 0=rhs (compiled form) */
  struct AlgebraicEquation {
    std::string rhs;
    Program form;
    Place where; /* its 0= line, for an error (a DAE that cannot be solved) */
  };
  int nsvar=0,naeqn=0;
  std::vector<AlgebraicVariable> svars;
  std::vector<AlgebraicEquation> aeqns;

  /* ---- the model's own values and options ---- */
  /* the parameters' values and the variables' initial conditions as the
     model gives them (the ones in use are the Session's: constants,
     last_ic) */
  std::vector<double> default_val;
  std::vector<double> default_ic;
  /* the initial values a model draws (an .odex's init that calls ran; any
     other init is a number at load): each variable (by name) and its
     formula, drawn in order once the model is set up (form_ode.cpp's
     set_initial_values); where its init is, for an error (no cause) */
  struct InitialValue {
    std::string name;
    odex::Expr formula;
    Place where;
    int index = -1; /* -1 until the builder resolves the state column */
  };
  std::vector<InitialValue> initial_values;
  /* a statement is a map, x(t+1)=... (disc: the method is discrete) */
  int is_a_map=0;
  /* its array initial values, x[j1..j2](0)=formula (extract_ic_data) */
  ArrayIcs array_ics;
  /* its options, each line (an .ode's @ line, whole as read; a .odex's
     options entry) as the name=value items it makes, and where (no cause),
     for a value an option refuses: load_eqn.cpp's set_internopts sets the
     numerics and plot settings from them (their current values are the
     Session's) */
  struct OptionLine {
    std::string text;
    std::vector<std::pair<std::string,std::string>> items;
    Place where;
  };
  std::vector<OptionLine> options;

  /* ---- delays, integral equations, Markov chains, networks ---- */
  /* the delay terms the parser compiled (delay(), a delayed network) */
  int ndelays=0;
  /* the integral equations' kernels (nkernel of them) */
  int nkernel=0;
  std::vector<KERNEL> kernels;

  /* a Markov variable's chain: nstates states and their values, and the
     nstates x nstates transitions as typed (trans) and compiled
     (command), or as numbers (fixed) when type is 1 (fixed for all time;
     0 when they depend on the state) */
  struct MarkovChain {
    std::vector<Program> command;
    std::vector<std::string> trans;
    std::vector<double> fixed;
    int nstates=0;
    std::vector<double> states;
    int type=0;
    std::string name;
  };
  /* the Markov variables' chains (nmarkov of them) */
  std::vector<MarkovChain> markov;
  /* the Wiener parameters' places in the parser's constants (nwiener) */
  std::vector<int> wiener;

  /* the variables' names by index: the ODEs (NODE), the Markov variables
     (NMarkov), then the aux quantities, NEQ in all; browse_data's added
     column takes the next one */
  std::vector<std::string> uvar_names;
  /* the parameters' names (NUPAR of them) */
  std::vector<std::string> upar_names;
  /* each parameter's place in the parser's constants (what its name finds
     there: the state event reads every parameter, without the lookups) */
  std::vector<int> upar_con;
  /* the user functions' names (NFUN of them) */
  std::vector<std::string> ufun_names;
  /* each user function's argument names (narg_fun[i] of them) */
  std::vector<std::vector<std::string>> ufun_args;

  /* ---- the networks and vectorizers (simplenet.cpp) ---- */
  int nnetwork=0,nvector=0;
  std::vector<Network> networks;
  std::vector<Vectorizer> vectors;

  /* ---- the user functions (expr_symbols.cpp), nfun of them ---- */
  int nfun=0;
  /* each one's argument count */
  std::vector<int> narg_fun;
  /* each one's formula as typed */
  std::vector<std::string> ufun_defs;
  /* each one compiled: the formula then ENDFUN, its argument count and
     ENDEXP */
  std::vector<Program> ufun_programs;
  /* room for user function index in each of the above (the functions have
     no limit: the vectors grow with what the model defines) */
  void room_for_function(int index) {
    const std::size_t size = static_cast<std::size_t>(index) + 1;
    if (ufun_names.size() >= size) return;
    ufun_names.resize(size);
    ufun_args.resize(size);
    narg_fun.resize(size);
    ufun_defs.resize(size);
    ufun_programs.resize(size);
  }
  /* ---- the source ---- */
  /* the model file's lines as read (strip_saveqn makes their control
     characters blanks at the end of the load) */
  std::vector<std::string> source;
  int nlines() const { return static_cast<int>(source.size()); }
  /* its statements as its reader read them (odex.h; an array's copies,
     each marked), in order, as the builder made them (in an .odex, a
     fixed quantity of parameters a derived one): xppautX --convert
     writes them (odex_convert.cpp) */
  std::vector<odex::Statement> statements;
  /* the files a statement's place (odex::Pos::file) indexes: the model's
     own first, then each file it includes (model_place, model_files.h) */
  std::vector<std::string> statement_files;
  /* W291: the load's single model-name index. The parser separately owns
     built-ins, argument slots and temporary expression symbols. Indices
     are -1 until assigned by the builder, or when that namespace excludes
     the name (a fixed quantity can also have an aux output column). */
  enum class NameKind { Variable, Markov, Fixed, Aux, Param, Const, Wiener,
                        Function, Table, Network, Vector, Algebraic, Derived, Set, Kernel };
  struct NameOrigin {
    std::string written;
    odex::Pos pos;
    std::string array;
  };
  struct Name {
    NameKind kind = NameKind::Variable;
    odex::Pos pos;
    std::string written, array;
    int arity = 0;
    int index = -1, column = -1, parameter = -1, set = -1;
    int parser_var = -1, parser_con = -1;
    std::unordered_map<std::string,int> set_spellings; /* exact .ode set spellings can share a folded name */
    bool declared = false, variable_declared = false, markov_declared = false;
    int function_statement = -1;
    int purity = -1; /* -1 unknown, 0 visiting/impure, 1 pure during load */
    bool purity_constant = false, purity_table = false, purity_derived = false;
    std::optional<std::pair<std::string,double>> load_constant;
    std::unordered_map<int,std::pair<std::string,std::string>> array_members; /* load's range group -> spelling, provenance */
    std::vector<NameOrigin> origins; /* load-only clash history, cleared on success */
  };
  std::unordered_map<std::string, Name> names;
  /* Foreign Markov headers match prefixes: probe only declared lengths,
     so a hostile long header does not cause quadratic substring work. */
  std::set<std::size_t> markov_name_lengths;
  const Name *name(std::string_view text) const;
  /* Declaration metadata is written only while loading; indices are the
     owner's category indices, with parser and output namespaces separate. */
  Name &define_name(std::string_view text,NameKind kind,int index,const odex::Pos &pos);
  /* Source lines indexed by model_files during the load; read-only in a run. */
  std::map<std::string, Lines, std::less<>> source_lines;
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
    /* a .odex set's settings, under its `options:` (name and value as the
       options block's entries): never a model name, whatever the name is */
    std::vector<std::pair<std::string,std::string>> settings;
    /* its line in the model, for an item that cannot be applied */
    Place place;
  };
  std::vector<InternalSet> intern_sets;
  /* the loaded file's path, as given ("console" for standard input) */
  std::string this_file;
  /* a model was loaded: the Model of a program started with no file (the
     start screen, W232) has none */
  bool loaded() const { return !this_file.empty(); }
  /* the model's own files as its load read them (model_files.h):
     this_file first, then each file included, each file table, the
     options file and --anifile's animation, in the order read; an AUTO
     file and a session file save them all (xpp_session.h) */
  std::vector<ModelFile> files;
  /* the .snapx the model was loaded from (an absolute path),
     whose saved copies are its files; empty for a model read from the
     disk */
  std::string saved_in;
  /* the paths outside the model's folder the user allowed while this model
     loaded, as files::resolve_path gives them (approve_model_path, model_files.h) */
  std::vector<std::string> outside_paths;
  /* during a load of a saved model, the scratch folder of the copies a
     reader of a FILE * reads (model_files.cpp) */
  std::shared_ptr<TempDir> saved_copies;
  /* the command line it was loaded with (argv, the program's name first):
     File > Reload loads it again (model_switch.h) */
  std::vector<std::string> command_line;
  /* the working directory it was loaded in, which the command line's
     relative paths name files of */
  std::string load_dir;
};


}

