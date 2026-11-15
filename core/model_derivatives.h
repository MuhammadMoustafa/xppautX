#pragma once
/* The exact Jacobian's derivatives of a model's programs (W260), made from the symbolic graph of
   expr_derivative.h; translate_model (model_native.cpp) writes them as C. C++ only. */
#include "expr_derivative.h"
#include "xpp_error.h"

#include <span>
#include <vector>

namespace xpp {

struct Session;
struct ModelProgram;

namespace expr {

/* The model's right-hand sides differentiated: entry k says that
   d f_row / d column is value, for each derivative not identically 0 */
struct ModelDerivatives {
    struct Entry {
        int row = 0, column = 0;
        Term value;
    };
    Symbolic graph;
    std::vector<Entry> entries;
};

/* The derivatives of the Session's model, or why it has none: a model
   with Markov variables, delays, integral equations, algebraic equations
   or random numbers, an operation with no derivative, a function table,
   a program translate_model did not compile (programs: model_programs'),
   or a model too large (the place the reason is at). */
Result<ModelDerivatives> differentiate_model(const xpp::Session &s, std::span<const ModelProgram> programs);

} // namespace expr
} // namespace xpp
