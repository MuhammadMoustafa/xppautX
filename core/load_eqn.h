#pragma once

#include <stdio.h>
#include "model_options.h"
#include "line_style.h"
#include "numerics_settings.h"
#include "solver.h"
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

/* The load (xpp_batch.cpp) hands the Model and Session it builds to what
   reads the model and its options (W47d3): */
/* the model's file (Model::this_file) named on the command line is kept;
   none (or a folder) is the error xpp::is_no_model (model_failed.h): the
   start screen's Open model asks, not the load */
void choose_model_file(Session &s);
void load_eqn(Session &s);
/* the defaults of the options no source set (set_option_defaults), then
   the storage, the solver and the initial values the model gives */
void set_all_vals(Session &s);
/* an @ line of the model (an .ode's, the .ode reader) kept in m's options,
   which set_internopts applies; -1 (logged, not kept) when it sets
   dll_lib or dll_fun */
int add_model_option(Model &m, std::string_view s1);
/* a .odex's options block, its entries kept in m's options, each at its
   own place (the file names the entries' Pos::file in files); a load error
   past the limit of options a model may set */
void add_model_options(Model &m, std::span<const odex::Option> entries, const std::vector<std::string> &files);
/* the model's options not yet applied (mask: the options unset before
   the model's, which its lines may set again), each through set_option
   (model_options.h) */
void set_internopts(Session &s, const OptionsSet *mask);
/* the torus settings read from or written to a set file (lunch-new.cpp) */
/* the model's internal sets: one added (set name {does}), one's
   "name=value ..." settings applied to s (extract_action for any such
   text, a comment's action), one setting */
void add_intern_set(Model &m, std::string_view name, std::string_view does,
                    std::vector<std::pair<std::string, std::string>> settings = {});
/* the model's sets and options lines again from the statements of its file
   read anew, once the file was edited (model_save.h): every set of the
   statements, an import's included, at its own place; the options block's
   entries (the sets and the options of the model before are dropped) */
void replace_intern_sets(Model &m, const odex::Parsed &parsed);
void replace_model_options(Model &m, const odex::Parsed &parsed);
/* all or nothing: every item is checked first, and the first that cannot
   be applied is the error, at where (the set's line in the model), with
   nothing applied. xppaut_names: XPPAUT's words for the options count too
   (the command line's settings); a model's own set and action do not take them */
Result<> extract_action(Session &s, std::string_view ptr, const Place &where, bool xppaut_names, bool graph_settings = false);
/* The scopes of a set's apply and of restoring defaults: all, the parameters,
   the initial values (the states), or one of the option groups a set may hold
   (set_groups, model_options.h) */
namespace set_scope {
inline constexpr std::string_view all = "all", parameters = "parameters", initial = "initial";
}
std::vector<std::string> set_scopes(bool model_wide);
/* one name=value of a set or of a file of values: a model name's (a parameter's
   or a state's) or a setting's, which no model name takes the place of; at is
   its line in its file (line 0: the set's; no file: the one the set is in) */
struct SetItem {
  std::string name, value;
  bool setting;
  Place at;
};
/* the items of scope (all of them for `all`), in order */
std::vector<SetItem> scoped_items(const Session &s, std::span<const SetItem> items, std::string_view scope);
/* items limited to scope applied as a patch over the current values, the plot
   settings to graph (the active one when none): every item checked first, the
   first that cannot be applied the error (at its line, or at where) with
   nothing changed; one undo step. command names the operation in an error.
   apply_named_set and Load values from a file are this. */
Result<> apply_values(Session &s, std::span<const SetItem> items, const Place &where, std::string_view scope, std::optional<int> graph,
                      std::string_view command);
/* the settings of option group now as their text (unquoted), each with its row, in the table's order: those with a member that the
   method and model use; the plot settings are those of graph (the active one when none) */
Result<std::vector<std::pair<const OptionRow *, std::string>>> option_group_values(Session &s, std::string_view group, std::optional<int> graph,
                                                                                    std::string_view command);
/* items (the group's settings, by name) applied as a patch, all or nothing, one undo step: apply_values for a group a set holds, the same
   check and write for a group of the session as a whole (diagram, export, interface, output) */
Result<> apply_option_group(Session &s, std::string_view group, std::span<const SetItem> items, const Place &where, std::optional<int> graph,
                            std::string_view command);
/* why scope is none of them (naming them), "" when it is */
std::string set_scope_problem(std::string_view scope, bool model_wide = false);
/* the index of the model's set called name (the spelling it was written in),
   -1 when it has none */
int intern_set_index(const Model &m, std::string_view name);
/* the scopes a set holds entries of (the parameters, the initial values, the option groups), each once, in the order its entries
   come: what a section's Sets list offers (W323) */
std::vector<std::string_view> scopes_held(const Session &s, std::size_t set);
/* Apply set: a patch over the current values, the model's set `name` limited
   to the entries of `scope` (the parameters, the initial values, one group of
   settings, or all); the plot settings go to graph (the active one when
   none). All or nothing: a bad entry, a set or a graph that does not exist is
   the error (at the set's line, or the command's place) and nothing changed.
   A parameter-only apply does not touch the initial values, whose formulas are
   not evaluated again. One undo step. */
Result<> apply_named_set(Session &s, std::string_view name, std::string_view scope, std::optional<int> graph);
/* Restore defaults: the model's own values back, for scope (as above) and, with
   a field, only that parameter, state or setting of it. A setting returns to
   the built-in value with the model's `options` lines over it. All or nothing,
   one undo step. */
Result<> restore_defaults(Session &s, std::string_view scope, std::string_view field, std::optional<int> graph);
/* every set of the model checked as apply_named_set would apply it, none
   applied: the first that cannot be is the error, at its line (a load, so a
   bad set stops it) */
Result<> check_intern_sets(Session &s);
void do_intern_set(Session &s, std::string_view name1, std::string_view value);
/* the current values of scope (as apply_named_set's) as a save writes them: every
   parameter, every state (the last state reached, stored, when last_state), and
   every setting of the groups a set holds that the scope covers, from graph (the
   active one when none) for the plot settings. The error when a scope or graph is
   none, or the last state was asked and nothing was run. */
Result<odex::Values> captured_values(Session &s, std::string_view scope, std::optional<int> graph, bool last_state, bool model_wide);
/* what a key of a set is in m: a parameter, a state, or neither */
odex::ValueKind value_kind(const Model &m, std::string_view name);

} // namespace xpp
