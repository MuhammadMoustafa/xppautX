#pragma once
/* The settings panels as data (W616): the option table's grouped rows as `hello`'s "options", which the page builds one panel per
   group from (docs/protocol.md "The settings panels"). Every label, section, kind, choice list, default, limit and help line is
   the row's; the page writes none. options_panels.cpp; the commands that read and write a panel's values are
   commands_options.cpp. */
#include <string>

namespace xpp {

struct OptionRow;
struct SessionParts; /* model_options.h */

/* whether row is a field of a panel: a .odex option with a value to show and set (the lists, `sliders` and `curves`, keep their
   own editors) */
bool option_in_panel(const OptionRow &row);

/* hello's "options" value: a list of {group, label, rows}, the group's name as words for its label, each row {key (its .odex key),
   section (the key's part before its last dot), label (the key's last part as words), kind, help, default and, as the row has
   them, choices, boolean, nonzero and the schema's limits}; the defaults are what `fresh`, the parts of a fresh Session of the model,
   holds once the rows' resets are applied to it (none before a model is loaded) */
std::string options_table_json(const SessionParts &fresh);

} // namespace xpp
