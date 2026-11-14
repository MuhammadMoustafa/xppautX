#pragma once
/* Where a model line is, for an error a run causes in it (W140). A
   statement's kind has its own, model_statement_place (odex.h). C++ only. */
#include "xpp_error.h"
#include "odex_expr.h"

#include <string_view>

namespace xpp {

struct Model; /* model.h */

/* Where a model line is, for an error it causes at run time (W140): the
   file of a statement's place (odex::Pos, its file one of
   ModelRecord::statement_files), its line and column and the line as written. */
Place model_place(const Model &m, const odex::Pos &pos);
/* where m defines name (any case): the equation, aux quantity, function,
   table, Markov variable, network or parameter line the parser read it
   from; where a load in progress is (xpp::Load::place(), session.h) when
   no line does, else an empty Place (a column the browser added) */
Place model_place(const Model &m, std::string_view name);

} // namespace xpp
