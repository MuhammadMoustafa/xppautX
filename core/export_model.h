#pragma once
/* Export the model a Session holds as the source of a program in another language (W764): the equations and the values the model
   has now (parameters, initial data, the integration settings) as a file that runs with no xppautX and writes the rows of a run.
   One translator reads the model's expression trees (model_native.h's model_ir, the Generator's: no formula text is parsed here)
   and writes it through a Language: a back end is a table of names and spellings and one template for the file, so another language
   is its table and template (export_python.cpp is the model). A model the language cannot say is refused, naming what and the model
   line, and no file is written. */
#include <array>
#include <span>
#include <string>
#include <string_view>

#include "xpp_error.h"

namespace xpp {

struct Session;       /* session.h */
struct SessionParts;  /* model_options.h */
class FileSystem;     /* xpp_fs.h */
struct ModelLoader;   /* model_load.h */
struct LoadServices;  /* model_file_types.h */

/* a function the language writes: the expression language's name for it (its symbol table's: "SIN", "^", ">") and its text,
   {0} and {1} the arguments' places */
struct Spelling {
    std::string_view name, text;
};

/* A back end. Texts with {0}, {1}, {2} are filled with the arguments' text; the file's template with the named parts the
   translator makes (export_model.cpp's script() lists them). */
struct Language {
    std::string_view name;      /* the language's own name: the model.export choice and --to's word */
    std::string_view extension; /* of the file written, with its dot */
    std::string_view comment;   /* what a comment line starts with, space included */
    std::span<const std::string_view> reserved; /* the words no model name may be: the language's own, and every name the file itself uses */
    std::string_view indent;    /* one level */
    /* expressions */
    std::string_view add, subtract, multiply, divide /* IEEE */, guarded_divide /* XPP's: a zero divisor is replaced */, select /* {0}==0 ? {1} : {2} */;
    std::string_view number_infinity, number_not_a_number, loop_counter /* an integer as a double */;
    std::span<const Spelling> functions; /* the math functions it has; any other is refused */
    /* statements: blocks open and close as these say (an empty closing is a language that closes by indenting) */
    std::string_view assign, loop_begin /* {0} counter, {1} first, {2} last (integers) */, loop_end, if_zero, else_branch, if_end, empty_block,
                     function_begin /* {0} name, {1} parameters */, function_return, function_end;
    std::string_view file; /* the template: one part for each thing of the model, and the run */
};

/* the languages there are: Python's */
const Language &python_language();
inline constexpr std::array<std::string_view, 1> language_names{"python"};
/* the language of that name, nullptr when there is none */
const Language *language_named(std::string_view name);

/* The Session's model as a program in language written to file in the model's folder, asking before it replaces one (as every
   save asks); the file written. The model the language cannot say (the first thing, with its model line) and a replace the
   user declined are errors: no file is written. */
Result<std::string> export_to_file(FileSystem &fs, const SessionParts &s, const Language &language, std::string_view file);
/* The same for a file asked for when none is given (the command's, the window's file dialog): the file written, or "" when the
   question was cancelled. */
Result<std::string> export_model(Session &s, const Language &language, std::string_view file);

/* --convert MODEL --to LANGUAGE: the model loaded as it is (a .ode converted first, as --convert alone does) and written
   beside itself, MODEL's name with the language's extension; the program's exit status */
int convert_to_language(FileSystem &fs, const ModelLoader &loader, const std::string &model, const Language &language, const LoadServices &services);

} // namespace xpp
