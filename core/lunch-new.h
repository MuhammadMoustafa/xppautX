#pragma once


#include <stdio.h>
#include "form_ode.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_error.h"
#include "xpp_io.h"    /* Lines */

namespace xpp {

struct Session; /* session.h */
struct Model;   /* model.h */

/* ---- XPPAUT's files, imported and never written ----
   XPPAUT's set file (.set) is positional lines, a value and then its
   label, read by their order: lunch-new.cpp reads it to be imported and
   nothing of ours writes it. A parameter file (.par) and an
   initial-conditions file (.ic) are lines of it too, read through
   xpp_io.h's Lines (whole(), real(), next(), heading()), the one way a
   file is read (W125). */

struct Settings; /* session_settings.h */

/* XPPAUT's set file at path read whole for s's model: every value and label
   checked, or the error at the line that is wrong; its model's equations
   after its last value ("RHS etc ...") are not read. Nothing of s changes. */
Result<Settings> read_xppaut_set(Session &s, std::string_view path);

/* Import XPPAUT session state: the set file at path applied to s whole (its
   parameters, initial conditions, numerics, delays, boundary conditions, the
   active graph, the run position, ...), or the error and nothing applied.
   Gives, and shows as one message, what it changed. The front end is shown it
   when redraw. */
Result<std::vector<std::string>> import_xppaut_state(Session &s, std::string_view path, bool redraw);

/* a parameter file's values (the model m's parameters, in order) or an
   initial-conditions file's (one per differential equation), read whole
   from path; the error at the line that is wrong */
Result<std::vector<double>> read_parameter_file(const Model &m, std::string_view path);
Result<std::vector<double>> read_ic_file(const Model &m, std::string_view path);
/* the two files of the values panel's Load: the model's parameters (.par) or its initial conditions (.ic) */
enum class ValueFile { parameters, initial_conditions };
inline constexpr std::array<std::string_view, 2> value_file_names = {"parameters", "initial_conditions"};
/* the parameters or the initial conditions of s read whole from the file fn as XPPAUT wrote it (--parfile, --icfile,
   ic.load, the values panel's Load) and applied as Load values from a file by scope applies a part (set_load.cpp,
   load_eqn.h's apply_values), or the error and nothing applied. An .ic holds the values alone, one per
   differential-equation variable in the model's order, exactly `node` of them, as XPPAUT's own io_ic_file always read
   (the Markov chains are not in this file, in XPPAUT or here: docs/manual 16-quick-reference.md). */
Result<> load_xppaut_values(Session &s, ValueFile kind, std::string_view fn);
/* Load of s's .par and .ic (docs/protocol.md "value.load_file"): file empty asks for one like Save data does,
   given skips the ask; gives the file read ("" when the user cancelled the question), as one undo step, all or
   nothing. */
Result<std::string> load_value_file(Session &s, ValueFile kind, std::string file);
/* the model's internal sets (name, whether --silent runs it, what it
   sets), its parameters' values in the file and its initial conditions,
   those asked for, each under its # heading, into name (--silent's
   --qsets/--qpars/--qics) */
void write_values_query(const Session &s, std::string_view name, bool sets, bool pars, bool ics);

/* File > Import XPPAUT session state (asks for the file), the session s's info file
   (File > Save info) */
void import_xppaut_state_command(Session &s);
void file_inf(Session &s);
/* the simulation information written to filename (Save info): false when declined; confirm replaces an existing file without asking */
Result<bool> write_info(Session &s, const std::string &filename, bool confirm);
void do_info(const Session &s, FILE *fp);

} // namespace xpp
