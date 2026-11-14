#pragma once
/* Load values from a file by scope (W321, docs/studies/w309-values-design/sets-design.md
   sections 6 and 7). A file of values is read whole and checked, which parts of it hold a
   scope's kind are offered, and the chosen part is checked against this model and applied
   as a patch (load_eqn.h's apply_values: the same as applying one of the model's own
   sets), or not at all. Nothing runs: no equations, no log file, no defaults applied, and
   the model's file is never changed. The sources:
   - an .odex (a model or a file of sets): its defaults (the par and init lines and the
     options a set may hold) and each of its sets;
   - XPPAUT's files, imported and never written: a .par (parameters), an .ic (initial
     conditions), a .set (its parameters, initial conditions and numerics settings; its
     graphs, delays, boundary conditions and run position come in only through Import
     XPPAUT session state, lunch-new.h). C++ only. */
#include "load_eqn.h"
#include "xpp_error.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>

namespace xpp {

struct Session; /* session.h */

/* the most parts (and bytes of their text) set.list_file_contents lists in one reply: a file of many sets is chosen by name */
inline constexpr std::size_t offered_parts_limit = 1000, offered_bytes_limit = 1024 * 1024;

/* which part of a file: its defaults, one of its sets, or the whole of an XPPAUT file */
enum class ValuesOrigin { defaults, set, file };
inline constexpr std::array<std::string_view, 3> values_origin_names = {"defaults", "set", "file"};

/* a part of a file of values: its entries (parameters, states and settings, each at its
   own line), where the part starts, and why it cannot be loaded when it cannot (an entry
   the model has no name for, a value that is not a number): the error is then the one
   loading it gives */
struct ValuesPart {
  ValuesOrigin origin = ValuesOrigin::file;
  /* a set's name; empty for the others */
  std::string name;
  std::vector<SetItem> items;
  Place where;
  std::optional<Error> problem;
};

struct ValuesSource {
  /* the kind of file, as its extension names it: odex, par, ic or set */
  std::string_view format;
  /* an .odex's text, whose lines an error of one of its entries shows (empty for the others) */
  std::string text;
  std::vector<ValuesPart> parts;
};

/* the file at path read whole and checked against s's model, or the error (a file that is
   none of the sources above, one that cannot be read within a bound, a syntax error at its
   line) */
Result<ValuesSource> read_values_source(Session &s, std::string_view path);

/* whether part has anything of scope's kind to load (or a problem to say) */
bool offers(const Session &s, const ValuesPart &part, std::string_view scope);

/* the part of the file at path that origin and name choose (name: a set's), limited to
   scope, applied as a patch over the current values, the plot settings to graph (the
   active one when none): all of it or nothing, one undo step, the error at the file and
   its line (and the line as written) */
Result<> load_values_from_file(Session &s, std::string_view path, std::string_view scope, ValuesOrigin origin, std::string_view name,
                               std::optional<int> graph);

/* xppautX --convert model.ode --statefile FILE: the model converted (an .odex is used as it is), then the values and
   numerics of FILE, an XPPAUT .par, .ic or .set, written into the .odex as its defaults, through the loading above and
   save_defaults: the one importer, written once; 0 when written, 1 (said why) when not */
int convert_state_file(const std::string &model, const std::string &state);

}
