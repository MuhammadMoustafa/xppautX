#ifndef XPP_EXPR_NATIVE_H
#define XPP_EXPR_NATIVE_H
#include "xpp_error.h"
namespace xpp {
struct Session;
/* Compile the entire completed model, or return its first unsupported place. */
Result<> compile_model(Session &s);
}
#endif
