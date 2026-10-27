#pragma once

#include "xpp_error.h"
#include "xpp_log.h"
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xpp {

struct Model;   /* model.h */
struct Session; /* session.h */

/* what the program shows its session in */
enum class Interface { window, browser, server };
/* --readset FILE or --with TEXT: a set of values read after the load */
enum class ExternalOptions { none, file, text };

/* the whole command line, read once by parse_command_line: the program's own
   switches (what interface, the port, --convert) and what a load takes (the
   model, its files and sets, options). A load keeps its own in its Model
   (Model::command_line), so File > Reload and a second session never share
   one. */
struct CommandLine {
  std::vector<std::string> arguments;          /* argv: the program's name first; File > Reload loads them again */
  int model_argument = -1;                     /* the model's index in arguments, or -1 when a file still needs choosing */
  std::string model_file;
  std::size_t load_options = 0;                /* options a load takes (all but the program's own switches) */
  /* the program's own switches */
  bool help = false, version = false, options_schema = false, setx_schema = false;
  bool convert = false, check = false, yes = false, no_open = false;
  std::optional<Interface> interface;          /* --browser/--web, --server: the last one given, never two different */
  std::optional<int> port;                     /* --port: 0 for any free port */
  /* what a load takes */
  bool silent = false, new_seed = false, make_plot = false, no_compile = false, no_output = false;
  bool query_sets = false, query_pars = false, query_ics = false;
  bool dry_run() const { return query_sets || query_pars || query_ics; } /* --qsets/--qpars/--qics: nothing is run */
  std::optional<XppLogLevel> log_threshold;    /* --verbose, --debug: the last one given */
  std::optional<bool> use_all_sets;            /* --internset 1 / 0 */
  std::optional<int> equilibria;               /* --equil N */
  std::string set_file, par_file, ic_file, out_file, out_place, ani_file;
  bool has_set_file = false, has_par_file = false, has_ic_file = false, has_out_file = false, has_ani_file = false;
  ExternalOptions external = ExternalOptions::none;
  std::string external_value;                  /* --readset's file or --with's text */
  std::vector<std::string> include, use_sets, skip_sets; /* --include, --uset, --rset in order */
  std::vector<std::pair<std::string_view, std::string>> options; /* --quiet ... through set_option, in order */

};

/* Read the whole command line in one pass: every option's spelling, argument
   and value checked; the first fault is an error naming the option, at its
   place in the command line. */
Result<CommandLine> parse_command_line(std::vector<std::string> arguments);
/* the command line's switches into the loading Session s and the model's
   file into its Model's this_file; the Model keeps cl. */
void apply_command_line(Session &s, CommandLine cl);
/* the internal sets of m the command line picked (--internset, --uset,
   --rset) into batch_options */
int if_needed_select_sets(const Model &m);
/* after a load, in every mode: the files the command line names
   (--setfile, --parfile, --icfile) and its --readset/--with options, into s */
void load_command_line_values(Session &s);

} // namespace xpp
