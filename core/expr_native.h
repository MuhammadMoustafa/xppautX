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
}
#endif
