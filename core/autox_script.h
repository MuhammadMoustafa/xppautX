#pragma once
/* An autoX script, name.autox (W306, #365; docs/manual/09-autox.md "Scripts", docs/protocol.md "autoX scripts"): the
   runs of one analysis of one model, named and in order, in .odex syntax (docs/odex.md "Settings syntax"):

     version: 4,
     model: "ab.odex",
     diagram: "branches.csv",
     runs: [
       { name: "steady", kind: equilibrium, replace_diagram: true, parameters: ["p0"], settings: { step_size: 0.01, parameter_max: 4 } },
       { name: "cycles", kind: periodic, start: { run: "steady", type: hopf, index: 1 }, parameters: ["p0"], settings: { step_size: -0.01 } },
     ]

   A run is one autox.run (commands_autox.cpp): `kind` and `switch`, its start (`start`: a point of an earlier run by
   its type and its index among that run's points of that type, since a label is assigned while a run goes and a
   script cannot know it; no `start`: the current state, settled first by `settle` {decimals, hold, maximum}), the
   `homoclinic` equilibria, `parameters` (the one or two model parameters it continues: autoX's axes, set as
   autox.configure_axes sets them before the run), and `settings`, the Numerics by their names for that run only (step_size's sign is the
   direction: both directions are two runs). The script reads whole and each run is checked against the command's own
   validator before anything is applied (all or nothing, W125); nothing computes on open. The model named is the one
   opened (xpp::load_model), the diagram file is written by --silent. C++ only. */
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>

#include "xpp_error.h"

namespace xpp {

inline constexpr std::string_view autox_script_extension = ".autox";
/* a script is a hand-written list of analyses; the bound only stops a hostile file's endless list (the lexer bounds its bytes) */
inline constexpr std::size_t autox_script_runs_limit = 1000;
inline constexpr std::size_t autox_script_bytes_limit = 1024 * 1024;

/* a start from a stored point: the `index`-th point of type `type` among the points run `run` stored */
struct AutoxScriptStart {
    std::string run, type;
    int index = 1;
};

struct AutoxScriptRun {
    std::string name;
    std::optional<AutoxScriptStart> from;
    /* the parameters it continues, one or two: the diagram's axes at the run (none: as they stand) */
    std::vector<std::string> parameters;
    /* it begins the diagram afresh (a later run's points replace the earlier runs') */
    bool replaces = false;
    /* the arguments of its autox.run, as `,"key":value` JSON members, without the start's type and index */
    std::string arguments;
    /* the run's line in the file, for the errors it gives */
    Place place;
};

struct AutoxScript {
    std::string file, model;
    /* the diagram's CSV file --silent writes after the runs; "" none */
    std::string diagram_file;
    std::vector<AutoxScriptRun> runs;
};

/* the diagram points a run stored: the indices [first, last) */
struct AutoxScriptPoints {
    int first = 0, last = 0;
};

/* a loaded script and how far it has run (the session's: AutoxState) */
struct AutoxScriptState {
    std::optional<AutoxScript> script;
    /* by run: the points it stored, none for a run not yet done or whose points a new diagram dropped */
    std::vector<std::optional<AutoxScriptPoints>> points;
};

/* run i has run, and its points are still the diagram's: what autox.script and autox.script_runs say alike */
inline bool autox_script_run_done(const AutoxScriptState &state, std::size_t i) { return i < state.points.size() && state.points[i].has_value(); }

struct Session; /* session.h */

/* the text of a script file read whole: the script, or the error at the line and value that is wrong */
Result<AutoxScript> autox_script_read(std::string_view text, const std::string &file);
/* the file's bytes, then autox_script_read */
Result<AutoxScript> autox_script_load(const std::string &file);

/* the autox.run line of a run: its start the stored point of type `type` and (diagram-wide) index `index`, or the current
   state when type is empty */
std::string autox_script_command(const AutoxScriptRun &run, std::string_view type, int index);
/* the same with the start the script names (its own type and index, as they stand in the run's file): what is checked at the load */
std::string autox_script_command(const AutoxScriptRun &run);
/* the autox.configure_axes line that sets a run's parameters, "" for a run that names none */
std::string autox_script_axes_command(const AutoxScriptRun &run);
/* every run's lines checked against the loaded model (its parameters' names, the settings' values): the first fault, at its run's line */
Result<> autox_script_check(Session &s, const AutoxScript &script);

} // namespace xpp
