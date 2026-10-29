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

/* ---- XPPAUT's set file (an import) and the files of values ----
   XPPAUT's set file (.set) is positional lines, a value and then its
   label, read by their order: lunch-new.cpp reads it to be imported and
   nothing of ours writes it. What ours keeps is the settings file, .setx
   (setx_file.h). A parameter file (.par) and an initial-conditions file
   (.ic) are lines of it too, read through xpp_io.h's Lines (whole(),
   real(), next(), heading()), the one way a file of ours is read (W125). */

/* "value   name" (a whole number), "value  name" (a number, %.16g): the
   parameter file's lines */
void write_whole(FILE *fp, int value, std::string_view name);
void write_real(FILE *fp, double value, std::string_view name);

/* The settings file at path applied to s, or the error, nothing applied: a
   .setx is read as it is (setx_file.h's read_settings). XPPAUT's set file
   (any other name; its model's equations follow its last value, "RHS etc
   ...", not read) is converted first: its values written as the .setx
   beside it, <base>.setx (a question before another text replaces one that
   is there, as .ode -> .odex asks: odex::write_converted), and that .setx
   is read and applied. Every named value must have its expected name.
   The front end is shown it when redraw. One message names what was
   done. */
Result<> import_settings(Session &s, std::string_view path, bool redraw);

/* a parameter file's values (the model m's parameters, in order) or an
   initial-conditions file's (one per differential equation), read whole
   from path; the error at the line that is wrong */
Result<std::vector<double>> read_parameter_file(const Model &m, std::string_view path);
Result<std::vector<double>> read_ic_file(const Model &m, std::string_view path);
/* the parameters or the initial conditions of s read whole from the file
   fn and applied, or the error and nothing applied (--parfile, --icfile,
   ic.load, the values panel's Load); written to it: false when the user
   declined to replace the file */
Result<> load_parameter_file_named(Session &s, std::string_view fn);
Result<> load_ic_file_named(Session &s, std::string_view fn);
Result<bool> write_parameter_file(Session &s, std::string_view fn);
Result<bool> write_ic_file(const Session &s, std::string_view fn);
/* the two files of the values panel's Save and Load: the model's parameters (.par) or its initial conditions (.ic) */
enum class ValueFile { parameters, initial_conditions };
inline constexpr std::array<std::string_view, 2> value_file_names = {"parameters", "initial_conditions"};
/* Save and Load of s's .par and .ic (docs/protocol.md "value.save_file"): file empty asks for one like Save data
   does, given skips the ask. A Save gives the file asked or given ("" when the user cancelled the question) and
   whether it was written (not when the user declined to replace it); a Load the file read ("" when the user
   cancelled the question), as one undo step, all or nothing. */
struct ValueFileSaved {
  std::string file;
  bool written = false;
};
Result<ValueFileSaved> save_value_file(Session &s, ValueFile kind, std::string file);
Result<std::string> load_value_file(Session &s, ValueFile kind, std::string file);
/* the model's internal sets (name, whether --silent runs it, what it
   sets), its parameters' values in the file and its initial conditions,
   those asked for, each under its # heading, into name (--silent's
   --qsets/--qpars/--qics) */
void write_values_query(const Session &s, std::string_view name, bool sets, bool pars, bool ics);

/* File > Import settings (asks for the file), the session s's info file
   (File > Save info) */
void import_settings_command(Session &s);
void file_inf(Session &s);
void do_info(const Session &s, FILE *fp);

} // namespace xpp
