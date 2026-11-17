#pragma once

#include <stdio.h>
#include "model_options.h"
#include "line_style.h"
#include "odex_write.h"
#include "xpp_error.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <cstddef>
namespace xpp { namespace odex { struct Option; } namespace odex { struct Parsed; } }


namespace xpp {

struct Model;   /* model.h */
struct Session; /* session.h */


/* s2 starts with s1 */
int msc(std::string_view s1, std::string_view s2);

/* the name=value items of an @ line as set_internopts reads them, or
   (set) of an internal set's "$ ..." as extract_action does: each
   item without a name or a value left out (@ total = 1 sets nothing) */
std::vector<std::pair<std::string, std::string>> option_items(std::string_view line, bool set,
                                                              std::vector<std::string> *ignored = nullptr);

/* an @ line of the model (an .ode's, the .ode reader) kept in m's options,
   which set_internopts applies; -1 (logged, not kept) when it sets
   dll_lib or dll_fun */
int add_model_option(Model &m, std::string_view s1);
/* a .odex's options block, its entries kept in m's options, each at its
   own place (the file names the entries' Pos::file in files); a load error
   past the limit of options a model may set */
void add_model_options(Model &m, std::span<const odex::Option> entries, const std::vector<std::string> &files);
/* the torus options read from or written to a set file (lunch-new.cpp) */
/* the model's internal sets: one added (set name {does}), one's
   "name=value ..." options applied to s (extract_action for any such
   text, a comment's action), one option */
void add_intern_set(Model &m, std::string_view name, std::string_view does,
                    std::vector<std::pair<std::string, std::string>> options = {});
/* the model's sets and options lines again from the statements of its file
   read anew, once the file was edited (model_save.h): every set of the
   statements, an import's included, at its own place; the options block's
   entries (the sets and the options of the model before are dropped) */
void replace_intern_sets(Model &m, const odex::Parsed &parsed);
void replace_model_options(Model &m, const odex::Parsed &parsed);
/* what a key of a set is in m: a parameter, a state, or neither */
odex::ValueKind value_kind(const Model &m, std::string_view name);

} // namespace xpp
