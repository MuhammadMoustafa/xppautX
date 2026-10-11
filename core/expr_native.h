#ifndef XPP_EXPR_NATIVE_H
#define XPP_EXPR_NATIVE_H
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
/* Refuse unsupported programs independently; unit/compiler failures return an error. */
Result<std::vector<Error>> compile_model(Session &s);
/* The model's exact Jacobian (W260), made on its first need (the model's
   own @ jacobian=exact at load, or the numerics being set to exact), never
   otherwise: true when it has one, else Model::jacobian.unavailable says
   why. Every user of Model::jacobian calls this first. */
bool ensure_exact_jacobian(Session &s);
}
#endif
