#pragma once
#include "xpp_error.h"
#include <cstdint>
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
   what is native (Program::compiled, Model::rhs_impurity, the subsystems) and leave MIR's input in
   Model::native_pending for build_native; a failure is the error. */
Result<std::vector<Error>> translate_model(Session &s);
/* Build the code Model::native_pending holds through MIR and give every program its function; a failure is logged
   and leaves the programs interpreted (Model::rhs_impurity says so). On the core thread, before any worker reads
   the code (MIR keeps static state and is not thread-safe). */
void build_native(Session &s);
/* The compile costs MIR some 20 thousand instructions for each node of the module (W715's measurements, the study's
   W715 section) and the interpreter some 20 instructions an operation more than native code, so the build is due
   once the interpreter has run this many operations for each node: a run that stops before has paid less than
   the compile would have cost, and one that goes on has paid at most half the compile again (lecar's
   183-operation right-hand side is interpreted some 350 times before the build, mu26's 2756-operation one 750
   times, kuramot100's sums, 100 thousand operations an evaluation, 10 times; mu26's 400 time units cost what
   they did with the build at the first evaluation, 2454 M instructions against 2455 M). */
constexpr std::int64_t INTERPRETED_OPS_PER_NODE=500;
/* my_rhs's check at each interpreted right-hand side while Model::native_pending: builds when due */
void build_native_when_due(Session &s);
/* for a run about to call the native code on threads: build it now; true when the right-hand side is pure and native */
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
