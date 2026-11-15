#pragma once
#include "xpp_error.h"
#include <vector>
#include <string>
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
   ascending, from the programs translate_model compiled: only for a model
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
/* Translate the model's programs to native code: refuse unsupported programs independently (the warnings), decide
   what is native (Program::compiled, Model::rhs_impurity, the subsystems) and start MIR's compile of them on a
   thread (Model::native_build); a failure is the error. */
Result<std::vector<Error>> translate_model(Session &s);
/* The native builds (Model::native_build, the exact Jacobian's): a build due starts its thread (now, when `wait`),
   and the code a thread made goes to its programs (every program its function, the Jacobian's its), waited for
   when `wait`, else only when it has finished (my_rhs, at the start of a right-hand side). A failure of the
   compile is logged and leaves them interpreted (Model::rhs_impurity says so for the programs). On the core
   thread. */
void build_native(Session &s, bool wait);
/* for a run about to call the native code on threads: wait for the code; true when the right-hand side is pure and native */
bool native_ready(Session &s);
/* Run translate_model when the load left it pending (Model::compile_pending), showing what it warns of with its place: the start of every
   computation (a command of kind compute) and of the exact Jacobian's making call it, so a model that is only opened costs no
   compile. A reload or Open model loads a new Model, pending again. */
void ensure_compiled(Session &s);
/* The model's exact Jacobian (W260), made on its first need (the model's
   own @ jacobian=exact at load, or the numerics being set to exact), never
   otherwise: true when it has one, else Model::jacobian.unavailable says
   why. Every user of Model::jacobian calls this first. */
bool ensure_exact_jacobian(Session &s);
}
