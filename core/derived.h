#pragma once

#include <string_view>

namespace xpp {

struct ParserState; /* expr.h */

/* the derived quantities of the parser p (the Model's, through its compiler tables): every one compiled (the load,
   once: one that does not fails it at its line), worked out again (after a
   parameter changed), one added (name = rhs) */
void compile_derived(ParserState &p);
void evaluate_derived(ParserState &p);
/* autoX's pure compiled path, with the worker's arrays; p is only read. */
void evaluate_derived_pure(const ParserState &p, double *constants, double *variables);
int add_derived(ParserState &p, std::string_view name, std::string_view rhs);

} // namespace xpp
