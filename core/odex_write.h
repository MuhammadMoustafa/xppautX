#pragma once
/* .odex text written, and a model file's text edited with the rest of it
   kept as written (W320, docs/studies/w309-values-design/sets-design.md
   section 9). C++ only.

   Two halves, one owner of how the text looks:
   - the layout the converter (odex_convert.cpp) and the edits both write:
     braced lists, the options block, a set;
   - the edits: a new set, one kind of a set replaced, the defaults rewritten,
     a set renamed or deleted. Each takes the text as it is, builds the new
     text by replacing only the statements it changes, and parses and checks
     the new text before returning it (a text that would not load again is an
     error and nothing else happens). Writing it to the disk, checking that the
     file did not change, and updating the live model are model_save.cpp's.
     The edits know no file and no Session: W322's session state in .odex
     syntax writes its sets, par/init lines and options groups the same way. */
#include "odex.h"
#include "xpp_error.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xpp::odex {

/* ---- the layout ---- */

/* items as { a, b, c }, on lines of their own when they would run past a
   line (indent: the column the line starts at) */
std::string braced(const std::vector<std::string> &items, std::size_t indent);

/* an entry of an options block as text, `key: value`, and its group */
struct GroupItem {
  std::string_view group;
  std::string text;
};

/* items as `group: { key: value, ... }`, one per group that has any, in the
   groups' order (indent: the column the group starts at) */
std::vector<std::string> grouped(const std::vector<GroupItem> &items, std::size_t indent);

/* options { group: { ... }, ... } at the start of a line ("" without items) */
std::string options_block(const std::vector<GroupItem> &items);

/* set "name" { items..., options: { group: { key: value } } } */
std::string set_block(std::string_view name, const std::vector<std::string> &items, const std::vector<GroupItem> &settings);

/* ---- the values a save writes ---- */

/* a setting as the options block writes it: its group, key and value text
   (a path or a text already in quotes) */
struct Setting {
  std::string group, key, value;
};

/* what is written, by kind: a kind is covered when the save is of it, and
   then it holds every current value of that kind (none: the kind is
   emptied). A name is as the model spells it, a value is a number's text. */
struct Values {
  bool parameters = false, states = false;
  std::vector<std::string> groups;
  std::vector<std::pair<std::string, std::string>> parameter_values, state_values;
  std::vector<Setting> settings;
  bool covers_group(std::string_view group) const;
};

/* what a key of a set is: the model decides (a parameter, a state, or
   neither) */
enum class ValueKind { other, parameter, state };
using Classify = std::function<ValueKind(std::string_view)>;

/* ---- the edits ---- */

/* where the lines of the old text went in the new: the places of a model are
   line numbers of the old */
struct LineMap {
  struct Change {
    int first = 0, removed = 0, added = 0; /* old lines first..first+removed became `added` lines */
  };
  std::vector<Change> changes; /* in order of first */
  int map(int line) const;
};

struct Edited {
  std::string text;
  LineMap lines;
};

/* Every edit takes m, a Model of its own to parse in (the parse reads the
   imports and may declare names; the live model is never given), the text
   and the name of its file. The error is at the statement or the line that is
   wrong. partial: the text is a file that holds sets for a model to load (an
   export), checked for its syntax but not against names it does not declare. */

/* `set name { ... }` with values appended at the end of the file */
Result<Edited> add_set(Model &m, std::string_view text, const std::string &file, std::string_view name, const Values &values, bool partial = false);
/* the kinds values covers replaced in the local set `name`, its other parts as
   they are */
Result<Edited> replace_in_set(Model &m, std::string_view text, const std::string &file, std::string_view name, const Values &values,
                              const Classify &classify, bool partial = false);
/* the kinds values covers written as the model's defaults: the par and init
   lines' values changed where they stand (an init line added for a state with
   none), the options block's groups replaced. Parameters and states are the
   ones whose value differs from the default; one that no line of this file
   declares is an error naming where it is declared. */
Result<Edited> set_defaults(Model &m, std::string_view text, const std::string &file, const Values &values);
Result<Edited> rename_set(Model &m, std::string_view text, const std::string &file, std::string_view from, std::string_view to);
Result<Edited> delete_set(Model &m, std::string_view text, const std::string &file, std::string_view name);

/* Parse (and, unless partial, check) text as a model of its own: the error
   is where it is wrong. */
Result<> check_text(Model &m, std::string_view text, const std::string &file, bool partial = false);

/* the local sets of text, by name (the statements' places only) */
Result<std::vector<std::pair<std::string, Pos>>> local_sets(Model &m, std::string_view text, const std::string &file);

/* text with the header of an export: "# Saved from file at time" */
std::string export_header(std::string_view file, std::string_view time);

}

