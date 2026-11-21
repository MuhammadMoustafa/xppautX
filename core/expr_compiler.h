#pragma once
/* What the formula compiler and the evaluators read and write of a model: its user functions, integral
   equation kernels, symbol counts, derived quantities and the index of its names. An expressions-owned struct
   the Model holds as `compiler` and the Session's ParserState points to (ParserState::compiler), so the
   expressions never include the Model (W743). */
#include "expr_program.h"
#include "xpp_error.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* a kernel that is a convolution (KERNEL's flag) */
#define CONV 2

/* An integral equation's kernel as the model declares it (expr_symbols.cpp
   add_kernel; volterra2.cpp alloc_v_memory compiles it while the model
   loads): K(t,t',u) with the constant mu, or a convolution kerexpr#expr
   (flag CONV). CompilerTables::kernels hold them; the running integrals are
   volterra2.cpp's (volterra.h's state). */
struct KERNEL {
  double mu=0.0;
  xpp::Program formula,kerform; /* expr and kerexpr compiled */
  xpp::Place where; /* the declaring line, including an anonymous inline kernel */
  int flag=0;
  std::string name,expr,kerexpr;
};

namespace xpp {

/* where a model name lives in the parser: its variable and its constant (-1 for none) */
struct NameSlots {
  int var = -1, con = -1;
};

/* The Model's index of the names its load recorded, which the symbol table keeps current and reads: an
   interface the expressions own and the Model's record implements (ModelRecord, model_record.h). */
struct NameIndex {
  virtual ~NameIndex() = default;
  /* the slots of `name` (as converted makes it); a read excludes the names of option sets, which no formula
     names, a write (as a symbol is added) finds them all; null when the model records no such name */
  virtual NameSlots *slots(std::string_view name, bool write) = 0;
  /* the place in the model's source that declares `name` */
  virtual Place place(std::string_view name) const = 0;
  /* the index of the lookup table `name` names, nothing when the model records no such name (or an option set's); -1 when it is another kind of name */
  virtual std::optional<int> table(std::string_view name) const = 0;
};

struct CompilerTables {
  /* the parser's variables (t, the model's variables, then its extras) */
  int nvar = 0;
  /* the delay terms the parser compiled (delay(), a delayed network) */
  int ndelays = 0;
  /* the model's formulas divide as IEEE does (1/0 is inf, 0/0 NaN), where an .ode's replace a zero divisor by
     ZERO_DIVISOR (expr_compile.cpp; docs/odex.md, question 1): the builder's copy of its statement list's
     Parsed::ieee_division, which only the .odex reader sets */
  bool ieee_division = false;

  /* ---- the user functions (expr_symbols.cpp), nfun of them ---- */
  int nfun = 0;
  /* each one's argument count */
  std::vector<int> narg_fun;
  /* each one's formula as typed */
  std::vector<std::string> ufun_defs;
  /* each one compiled: the formula then ENDFUN, its argument count and ENDEXP */
  std::vector<Program> ufun_programs;
  /* the user functions' names (nfun of them) */
  std::vector<std::string> ufun_names;
  /* each user function's argument names (narg_fun[i] of them) */
  std::vector<std::vector<std::string>> ufun_args;
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

  /* the integral equations' kernels (nkernel of them) */
  int nkernel = 0;
  std::vector<KERNEL> kernels;

  /* the networks and vectorizers the model holds (simplenet.cpp makes them): the most a program's network or
     vectorizer operand may name */
  std::size_t network_count = 0, vector_count = 0;

  /* ---- the derived quantities (derived.cpp): each one's constant (its
     place in the parser's constants) and formula, as text and compiled,
     worked out in this order whenever a parameter changes ---- */
  struct DerivedQuantity {
    int index = 0;
    std::string rhs;
    Program form;
    Place where; /* its line, for an error */
  };
  std::vector<DerivedQuantity> derived;
  /* one native function that works all `native_derived_count` of them out in order, writing each into its constant (the Generator's
     derived; null while they are interpreted), and the IR nodes one call evaluates (ParserState::native_work). A fit's or a
     parameter change's work is one call, not one a formula (W846); used only while derived.size() is that count. */
  void (*native_derived)(double *c, double *v, ParserState *S, const double *unused) = nullptr;
  std::uint64_t native_derived_nodes = 0;
  std::size_t native_derived_count = 0;
  bool derived_native() const { return native_derived && native_derived_count==derived.size(); }

  /* the model's name index (the Model sets it to its record) */
  NameIndex *names = nullptr;
};

}
