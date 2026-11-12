#pragma once
#include "expr_ir.h"
#include "xpp_error.h"
#include <vector>
namespace xpp {
struct Session;
struct Model;
struct Program;
/* One inventory for compilation and its parity checks; symbols contain only
   owner-defined kinds and numeric indices, never names from a model. */
struct ModelProgram {
    Program *code;
    std::string symbol;
    Place place;
    int user_index=-1;
    bool dynamic_operands=false;
};
std::vector<ModelProgram> model_programs(Model &m);
/* The states each equation of m reads, through the fixed variables it
   reads, itself included (a map's f(x)-x reads its own state), each list
   ascending, from the programs compile_model compiled: only for a model
   whose Model::rhs_impurity holds. The difference Jacobian's groups and
   the independent subsystems (Model::subsystems) are made from it. */
std::vector<std::vector<int>> equation_reads(const Model &m);
/* What joins every state of s's model into one subsystem under the run
   (the numerics' subsystem_split `run`: a Poincare section or stop in use),
   or "". Model::subsystems is the equations' own, the one the solvers factor
   by; the reports read the split through this. */
std::string run_joined_by(const Session &s);
/* s's independent subsystems in a line: how many and which states each
   holds, or the one the model is and what joins its states (the info
   file, the load's log, --check) */
std::string describe_subsystems(const Session &s);
/* the subsystems the model has under s's subsystem_split: several, or not */
bool has_subsystems(const Session &s);
/* Refuse unsupported programs independently; unit/compiler failures return an error. */
Result<std::vector<Error>> compile_model(Session &s);
/* Run compile_model when the load left it pending (Model::compile_pending), showing what it warns of with its place; the start of every
   computation (a command of kind compute) and of the exact Jacobian's making call it, on the core thread before any worker reads the code
   (TinyCC has static state and is not thread-safe). A reload or Open model loads a new Model, pending again. */
void ensure_compiled(Session &s);
/* W711 spike: s's right-hand side function p_rhs as the Generator's trees (expr_ir.h) for a back end that takes
   no C text. It walks the model's programs as compile_model does but compiles no C, so the model is left with no
   native code (interpreted): the spike that calls it compiles nothing in the core. RhsIr::unsupported says
   when the trees are not to be lowered. */
Result<RhsIr> rhs_ir(Session &s);
/* The model's exact Jacobian (W260), made on its first need (the model's
   own @ jacobian=exact at load, or the numerics being set to exact), never
   otherwise: true when it has one, else Model::jacobian.unavailable says
   why. Every user of Model::jacobian calls this first. */
bool ensure_exact_jacobian(Session &s);
}
