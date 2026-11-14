#pragma once
/* The pieces of the statement list the Model itself holds (odex.h has the
   rest): a place in a model and an .odex expression. C++ only. */
#include "xpp_error.h"

#include <string>
#include <utility>
#include <vector>

namespace xpp::odex {

/* a place in a model: which of Parsed::files, the line and the column
   (both from 1) */
struct Pos {
  int file = 0;
  int line = 0;
  int col = 0;
};

/* the problem cause at pos of file, as the load's own error
   (xpp::Error, xpp_error.h): what, and where */
inline Error error_at(std::string file, Pos pos, std::string cause)
{
  return Error{"model", std::move(cause), Place{std::move(file), pos.line, pos.col}};
}

/* an expression */
struct Expr {
  enum class Kind {
    Text,   /* an .ode formula (text) as the .ode reader keeps it: the
               expression engine compiles it as written, .ode's precedence
               and all */
    Number, /* value, text as written */
    Name,   /* text; primed for a name written x' */
    Neg,    /* -args[0] */
    Not,    /* not args[0] */
    Binary, /* args[0] text args[1]: or and < > <= >= == != + - * / mod ^ */
    If,     /* if args[0] then args[1] else args[2] */
    Call,   /* text(args...), arg_names[i] "" or the name of a named one */
    Index   /* args[0][args[1]] */
  };
  Kind kind = Kind::Number;
  Pos pos;
  double value = 0;
  std::string text;
  bool primed = false;
  std::vector<Expr> args;
  std::vector<std::string> arg_names;
};

} // namespace xpp::odex
