#pragma once
/* What a load recorded about where a Model came from and how it was written
   (W727): the statements its readers read, its name index, the files it
   read and the command line that started it. A load and what writes the
   model back (--convert, a save of its file, a session file) use it; what
   runs a Model reads Model alone, so these types (odex.h's statements,
   the file bytes, the command line) stay out of its headers. A Model holds
   one, Model::record, made with the Model. C++ only. */
#include "odex_expr.h"
#include "model_file_types.h"
#include "comline.h"
#include "xpp_io.h"
#include "xpp_zip.h"

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <utility>
#include <vector>

namespace xpp {
namespace odex { struct Statement; } /* odex.h */

struct ModelRecord {
  ModelRecord();
  ~ModelRecord(); /* odex.h's Statement is complete where it is defined (model_files.cpp) */

  /* the initial values a model draws (an .odex's init that calls ran; any
     other init is a number at load): each variable (by name) and its
     formula, drawn in order once the model is set up (form_ode.cpp's
     set_initial_values); where its init is, for an error (no cause) */
  struct InitialValue {
    std::string name;
    odex::Expr formula;
    Place where;
    int index = -1; /* -1 until the builder resolves the state column */
  };
  std::vector<InitialValue> initial_values;
  /* its statements as its reader read them (odex.h; an array's copies,
     each marked), in order, as the builder made them (in an .odex, a
     fixed quantity of parameters a derived one): xppautX --convert
     writes them (odex_convert.cpp) */
  std::vector<odex::Statement> statements;
  /* the files a statement's place (odex::Pos::file) indexes: the model's
     own first, then each file it includes (model_place, model_files.h) */
  std::vector<std::string> statement_files;
  /* W291: the load's single model-name index. The parser separately owns
     built-ins, argument slots and temporary expression symbols. Indices
     are -1 until assigned by the builder, or when that namespace excludes
     the name (a fixed quantity can also have an aux output column). */
  enum class NameKind { Variable, Markov, Fixed, Aux, Param, Const, Wiener, Ou,
                        Function, Table, Network, Vector, Algebraic, Derived, Set, Kernel };
  struct NameOrigin {
    std::string written;
    odex::Pos pos;
    std::string array;
  };
  struct Name {
    NameKind kind = NameKind::Variable;
    odex::Pos pos;
    std::string written, array;
    int arity = 0;
    int index = -1, column = -1, parameter = -1, set = -1;
    int parser_var = -1, parser_con = -1;
    std::unordered_map<std::string,int> set_spellings; /* exact .ode set spellings can share a folded name */
    bool declared = false, variable_declared = false, markov_declared = false;
    int function_statement = -1;
    int purity = -1; /* -1 unknown, 0 visiting/impure, 1 pure during load */
    bool purity_constant = false, purity_table = false, purity_derived = false;
    std::optional<std::pair<std::string,double>> load_constant;
    std::unordered_map<int,std::pair<std::string,std::string>> array_members; /* load's range group -> spelling, provenance */
    std::vector<NameOrigin> origins; /* load-only clash history, cleared on success */
  };
  std::unordered_map<std::string, Name> names;
  /* Foreign Markov headers match prefixes: probe only declared lengths,
     so a hostile long header does not cause quadratic substring work. */
  std::set<std::size_t> markov_name_lengths;
  const Name *name(std::string_view text) const;
  /* Declaration metadata is written only while loading; indices are the
     owner's category indices, with parser and output namespaces separate. */
  Name &define_name(std::string_view text,NameKind kind,int index,const odex::Pos &pos);
  /* Source lines indexed by model_files during the load; read-only in a run. */
  std::map<std::string, Lines, std::less<>> source_lines;
  /* the model's own files as its load read them (model_files.h):
     this_file first, then each file included, each file table, the
     options file and the animation's file, in the order read; an autoX
     file and a session file save them all (xpp_session.h) */
  std::vector<ModelFile> files;
  /* a Model made to parse a file of values in (set_load.cpp, scratch_model) holds the most bytes its files may hold in all */
  std::size_t files_bytes_limit = zip::archive_bytes_limit;
  /* the command line it was loaded with, read once (comline.h): File >
     Reload loads its arguments again (model_switch.h) */
  CommandLine command_line;
};

} // namespace xpp
