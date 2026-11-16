#pragma once
#include "expr_ir.h"
#include "xpp_error.h"
#include <cstddef>
#include <vector>
#include <string>
namespace xpp {
struct SessionParts; /* model_options.h */
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
std::string run_joined_by(const SessionParts &s);
/* s's independent subsystems in a line: how many and which states each
   holds, or the one the model is and what joins its states (the info
   file, the load's log, --check) */
std::string describe_subsystems(const SessionParts &s);
/* the subsystems the model has under s's subsystem_split: several, or not */
bool has_subsystems(const SessionParts &s);
/* A model's programs as the Generator's expression trees, for a back end that writes them as text (export_model.cpp), without
   compiling them or changing the model: the user functions, then each program a run evaluates in its place in the
   run (an equation's right-hand side, a fixed variable's value, an aux column, a derived parameter's formula),
   each one function of ir.functions with the model's name for it and its line (`origin`, by function). */
struct ModelIr {
    IrModule ir;
    struct Origin {
        std::string name;
        Place place;
    };
    std::vector<Origin> origin;
    std::vector<size_t> equations, fixed, aux; /* the function of each ODE, fixed variable and aux column, in the model's order */
    /* The equations that came from one array line of the model (`v[j]' = ... for j in 1..200`: the reader expanded it into one
       equation for each index, odex.h's ArrayCopy), consecutive in `equations` from `first`, with the line's index and range
       (`index` runs from `low` to `high` in steps of `step`; copy n of the line has low + n * step). Only an ODE's lines are kept, and
       only those of two copies or more; a back end that writes arrays reads the copies' trees to see what they share. */
    struct ArrayLine {
        size_t first, count;
        std::string index;
        int low, high, step;
    };
    std::vector<ArrayLine> arrays;
    struct Derived {
        size_t function;
        int constant; /* the parser's constant the formula's value is */
    };
    std::vector<Derived> derived; /* in the order a parameter change works them out */
};
Result<ModelIr> model_ir(const SessionParts &s);
/* Translate the model's programs to native code: refuse unsupported programs independently (the warnings), decide
   what is native (Program::compiled, Model::rhs_impurity, the subsystems) and start MIR's compile of them on a
   thread (Model::native_build); a failure is the error. */
Result<std::vector<Error>> translate_model(const SessionParts &s);
/* A loaded model translated (translate_model, W260's exact Jacobian when the
   numerics ask for it): the warnings the compiler gave, each with its place,
   or the error of a model that asks for an exact Jacobian it has none of.
   A load logs the warnings; --check reports them. */
Result<std::vector<Error>> compile_loaded(const SessionParts &s);
/* The native builds (Model::native_build, the exact Jacobian's): a build due starts its thread (now, when `wait`),
   and the code a thread made goes to its programs (every program its function, the Jacobian's its), waited for
   when `wait`, else only when it has finished (my_rhs, at the start of a right-hand side). A failure of the
   compile is logged and leaves them interpreted (Model::rhs_impurity says so for the programs). On the core
   thread. */
void build_native(const SessionParts &s, bool wait);
/* for a run about to call the native code on threads: wait for the code; true when the right-hand side is pure and native */
bool native_ready(const SessionParts &s);
/* Run translate_model when the load left it pending (Model::compile_pending), showing what it warns of with its place: the start of every
   computation (a command of kind compute) and of the exact Jacobian's making call it, so a model that is only opened costs no
   compile. A reload or Open model loads a new Model, pending again. */
void ensure_compiled(const SessionParts &s);
/* The model's exact Jacobian (W260), made on its first need (the model's
   own @ jacobian=exact at load, or the numerics being set to exact), never
   otherwise: true when it has one, else Model::jacobian.unavailable says
   why. Every user of Model::jacobian calls this first. */
bool ensure_exact_jacobian(const SessionParts &s);
}
