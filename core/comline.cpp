#include "model.h"
#include "session.h"
#include "comline.h"
#include "lunch-new.h"
#include "xpp_log.h"
/* the command line of xppautX */
#include "xpp_batch.h"
#include "xpp_globals.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xpp {

namespace {

/* what the command line asked for, read in one pass (parse_command_line) and
   applied to the loading Session by do_comline */
enum class ExternalOptions { none, file, text }; /* --readset FILE, --with TEXT */

struct CommandLine {
  int model_argument = -1;
  std::string model_file;
  bool silent = false, new_seed = false, make_plot = false, no_compile = false, no_output = false;
  bool query_sets = false, query_pars = false, query_ics = false;
  std::optional<XppLogLevel> log_threshold;                         /* --verbose, --debug: the last one given */
  std::optional<bool> use_all_sets;                          /* --internset 1 / 0 */
  std::optional<int> equilibria;                             /* --equil N */
  std::string set_file, par_file, ic_file, out_file, out_place, ani_file;
  bool has_set_file = false, has_par_file = false, has_ic_file = false, has_out_file = false, has_ani_file = false;
  ExternalOptions external = ExternalOptions::none;
  std::string external_value;                                /* --readset's file or --with's text */
  std::vector<std::string> include, use_sets, skip_sets;     /* --include, --uset, --rset in order */
  std::vector<std::pair<std::string_view, std::string>> options; /* --quiet ... through set_option, in order */
};

/* the options' ids (named, never numbers) */
enum class Opt { silent, no_compile, newseed, setfile, parfile, outfile, icfile, internset, uset, rset, include,
                 qsets, qpars, qics, quiet, logfile, anifile, mkplot, plotfmt, noout, dfdraw, ncdraw, readset, with,
                 equil, verbose, debug };

struct CommandOption {
  std::string_view name;
  Opt id;
  bool argument;
  bool repeatable; /* given again adds (--include, --uset, --rset); otherwise a second one is an error */
};

constexpr CommandOption my_cmd[] = {
  {"--silent", Opt::silent, false, true},
  {"--no-compile", Opt::no_compile, false, true},
  {"--newseed", Opt::newseed, false, true},
  {"--setfile", Opt::setfile, true, false},
  {"--parfile", Opt::parfile, true, false},
  {"--outfile", Opt::outfile, true, false},
  {"--icfile", Opt::icfile, true, false},
  {"--internset", Opt::internset, true, false},
  {"--uset", Opt::uset, true, true},
  {"--rset", Opt::rset, true, true},
  {"--include", Opt::include, true, true},
  {"--qsets", Opt::qsets, false, true},
  {"--qpars", Opt::qpars, false, true},
  {"--qics", Opt::qics, false, true},
  {"--quiet", Opt::quiet, true, false},
  {"--logfile", Opt::logfile, true, false},
  {"--anifile", Opt::anifile, true, false},
  {"--mkplot", Opt::mkplot, false, true},
  {"--plotfmt", Opt::plotfmt, true, false},
  {"--noout", Opt::noout, false, true},
  {"--dfdraw", Opt::dfdraw, true, false},
  {"--ncdraw", Opt::ncdraw, true, false},
  {"--readset", Opt::readset, true, false},
  {"--with", Opt::with, true, false},
  {"--equil", Opt::equil, true, false},
  {"--verbose", Opt::verbose, false, true},
  {"--debug", Opt::debug, false, true},
};

/* switches main consumes before any model loads (xppautx_main.cpp) */
constexpr std::string_view main_switches[] = {"--server", "--browser", "--help", "--version", "--port", "--no-open", "--web", "--yes"};

const CommandOption *command_option(std::string_view word)
{
  for (const CommandOption &option : my_cmd)
    if (option.name == word) return &option;
  return nullptr;
}

bool is_set_name(const std::vector<std::string> &sets, const std::string &nam)
{
  return std::find(sets.begin(), sets.end(), nam) != sets.end();
}

void add_set(std::vector<std::string> &sets, const std::string &nam)
{
  if(!is_set_name(sets,nam)) sets.push_back(nam);
}

/* the one pass over the command line: every option checked, its argument
   read and validated where it stands; the first fault is an error naming
   the option and its place */
Result<CommandLine> parse_command_line(int argc, char **argv)
{
  CommandLine cl;
  std::vector<const CommandOption *> seen;
  for (int i = 1; i < argc; ++i) {
    const std::string_view word(argv[i]);
    const Place place{"command line", i, 0, std::string(word)};
    if (!word.starts_with('-')) {
      if (!cl.model_file.empty()) return fail("options", xpp::format("unexpected model argument {}", word), place);
      cl.model_file = word;
      cl.model_argument = i;
      continue;
    }
    const CommandOption *option = command_option(word);
    if (!option) {
      if (!word.starts_with("--")) {
        const std::string spelling = "-" + std::string(word);
        if (command_option(spelling) || std::find(std::begin(main_switches), std::end(main_switches), spelling) != std::end(main_switches))
          return fail("options", xpp::format("{} is {}", word, spelling), place);
      }
      return fail("options", xpp::format("no such option {}", word), place);
    }
    if (!option->repeatable) {
      if (std::find(seen.begin(), seen.end(), option) != seen.end())
        return fail("options", xpp::format("{} given twice", word), place);
      seen.push_back(option);
    }
    std::string value;
    if (option->argument) {
      if (++i >= argc) return fail("options", xpp::format("{} needs an argument", word), place);
      value = argv[i];
    }
    const auto whole_number = [&](int &out) -> Result<> {
      if (!parse_int(value, out))
        return fail("options", xpp::format("{} needs a whole number, not \"{}\"", word, value), place);
      return {};
    };
    switch (option->id) {
    case Opt::silent: cl.silent = true; break;
    case Opt::no_compile: cl.no_compile = true; break;
    case Opt::newseed: cl.new_seed = true; break;
    case Opt::mkplot: cl.make_plot = true; break;
    case Opt::noout: cl.no_output = true; break;
    case Opt::qsets: cl.silent = cl.query_sets = true; break;
    case Opt::qpars: cl.silent = cl.query_pars = true; break;
    case Opt::qics: cl.silent = cl.query_ics = true; break;
    case Opt::verbose: cl.log_threshold = XPP_LOG_INFO; break;
    case Opt::debug: cl.log_threshold = XPP_LOG_DEBUG; break;
    case Opt::setfile: cl.set_file = value; cl.has_set_file = true; break;
    case Opt::parfile: cl.par_file = value; cl.has_par_file = true; break;
    case Opt::icfile: cl.ic_file = value; cl.has_ic_file = true; break;
    case Opt::anifile: cl.ani_file = value; cl.has_ani_file = true; break;
    case Opt::outfile:
      cl.out_file = value;
      cl.has_out_file = true;
      cl.out_place = xpp::format("{} {}", word, value);
      break;
    case Opt::internset: {
      int n = 0;
      if (const Result<> r = whole_number(n); !r) return std::unexpected(r.error());
      if (n != 0 && n != 1) return fail("options", xpp::format("{} must be 0 or 1, not {}", word, n), place);
      cl.use_all_sets = n == 1;
      break;
    }
    case Opt::equil: {
      int n = 0;
      if (const Result<> r = whole_number(n); !r) return std::unexpected(r.error());
      cl.equilibria = n;
      break;
    }
    case Opt::uset: add_set(cl.use_sets, value); break;
    case Opt::rset: add_set(cl.skip_sets, value); break;
    case Opt::include: cl.include.push_back(value); break;
    case Opt::quiet: cl.options.emplace_back("QUIET", value); break;
    case Opt::logfile: cl.options.emplace_back("LOGFILE", value); break;
    case Opt::plotfmt: cl.options.emplace_back("PLOTFMT", value); break;
    case Opt::dfdraw: cl.options.emplace_back("DFDRAW", value); break;
    case Opt::ncdraw: cl.options.emplace_back("NCDRAW", value); break;
    case Opt::readset:
    case Opt::with: {
      if (cl.external != ExternalOptions::none)
        return fail("options", xpp::format("{} given with {}: one of --readset and --with", word,
                                           cl.external == ExternalOptions::file ? "--readset" : "--with"), place);
      cl.external = option->id == Opt::readset ? ExternalOptions::file : ExternalOptions::text;
      cl.external_value = value;
      break;
    }
    }
  }
  return cl;
}

/* what the last do_comline read: the files and sets the load applies later */
CommandLine applied;

} // namespace

std::vector<std::string> include_files;
bool querysets = false, querypars = false, queryics = false, dryrun = false, newseed = false;

Result<> check_command_line(int argc, char **argv)
{
  if (const Result<CommandLine> cl = parse_command_line(argc, argv); !cl) return std::unexpected(cl.error());
  return {};
}

int do_comline(xpp::Session &s, int argc, char **argv)
{
  Result<CommandLine> parsed = parse_command_line(argc, argv);
  if (!parsed) throw LoadFailed{parsed.error()};
  /* what a command line before asked for (a load before this one: File >
     Open model, Reload) goes; this one says it again if it does */
  applied = std::move(*parsed);
  const CommandLine &cl = applied;
  s.got_file = 0;
  if (!cl.model_file.empty()) {
    s.model().this_file = cl.model_file;
    s.got_file = 1;
  }
  include_files = cl.include;
  querysets = cl.query_sets;
  querypars = cl.query_pars;
  queryics = cl.query_ics;
  dryrun = cl.query_sets || cl.query_pars || cl.query_ics;
  if (cl.new_seed) {
    xpp::log(XPP_LOG_INFO, "Random number seed changed\n");
    newseed = true;
  }
  if (cl.silent) batch_options.enabled = 1;
  if (cl.make_plot) s.integrator.make_plot_flag = 1;
  if (cl.no_compile) program.compile = false;
  if (cl.no_output) s.integrator.suppress_out = 1;
  if (cl.log_threshold) xpp::log_set_threshold(*cl.log_threshold);
  if (cl.has_out_file) {
    s.batch_out_file = cl.out_file;
    batch_options.user_out_file = cl.out_file;
    s.output_place = Place{"", 0, 0, cl.out_place};
  }
  if (cl.has_ani_file) {
    s.animation.options.file = cl.ani_file;
    s.animation.options.use_file = 1;
  }
  if (cl.use_all_sets) batch_options.use_intern_sets = *cl.use_all_sets ? 1 : 0;
  if (cl.equilibria) {
    batch_options.equilibria = *cl.equilibria;
    xpp::log(XPP_LOG_INFO, " Batch equilibria {:d} \n", batch_options.equilibria);
  }
  for (const auto &[name, value] : cl.options) set_option(s, name, value, 1, NULL);
  return cl.model_argument;
}

static int if_needed_load_ext_options(xpp::Session &s)
{
  if (applied.external == ExternalOptions::none) return 1;
  if (applied.external == ExternalOptions::file) {
    const std::string &file = applied.external_value;
    /* the file's first line, whatever its length */
    xpp::LineReader lr(file.c_str());
    if(!lr){
      xpp::log(XPP_LOG_WARN, "{} external set not found\n",file);
      return 0;
    }
    std::string myopts(lr.next().value_or(std::string_view()));
    xpp::log(XPP_LOG_DEBUG, "Got this string: {{{}}}\n",myopts);
    if(const xpp::Result<> r=extract_action(s,("$ "+myopts),xpp::Place{file,1,0,myopts},true);!r){
      xpp::log(XPP_LOG_ERROR, "{}\n",r.error().text());
      return 0;
    }
    return 1;
  }
  if(const xpp::Result<> r=extract_action(s,("$ "+applied.external_value),xpp::Place{},true);!r){
    xpp::log(XPP_LOG_ERROR, "{}\n",r.error().text());
    return 0;
  }
  return 1;
}

int if_needed_select_sets(const xpp::Model &m)
{
  if (applied.use_sets.empty() && applied.skip_sets.empty() && !applied.use_all_sets) return 1;
  const std::vector<xpp::Model::InternalSet> &sets=m.intern_sets;
  std::vector<int> &use=batch_options.intern_set_use;
  int &used=batch_options.intern_sets_used;
  use.assign(sets.size(),1);
  for(std::size_t j=0;j<sets.size();j++)
  {
    const std::string &name=sets[j].name;
    use[j]=batch_options.use_intern_sets;
    used+=batch_options.use_intern_sets;
    if (is_set_name(applied.use_sets,name))
    {
      xpp::log(XPP_LOG_INFO, "Internal set {} was included\n",name);
      if (use[j]==0){used++;}
      use[j]=1;
    }
    if (is_set_name(applied.skip_sets,name))
    {
      xpp::log(XPP_LOG_INFO, "Internal set {} was excluded\n",name);
      if (use[j]==1){used--;}
      use[j]=0;
    }
  }
  xpp::log(XPP_LOG_INFO, "A total of {:d} internal sets will be used\n",used);
  return 1;
}

static int if_needed_load_set(xpp::Session &s)
{
  if(!applied.has_set_file) return 1;
  if(const xpp::Result<> r=import_settings(s,applied.set_file,true);!r)
  {
    xpp::log(XPP_LOG_ERROR, "{}\n",r.error().text());
    return 0;
  }
  return 1;
}

void load_command_line_values(xpp::Session &s)
{
  if_needed_load_set(s);
  if(applied.has_par_file){
    xpp::log(XPP_LOG_INFO, "Loading external parameter file: {}\n",applied.par_file);
    load_parameter_file_named(s,applied.par_file);
  }
  if(applied.has_ic_file){
    xpp::log(XPP_LOG_INFO, "Loading external initial condition file: {}\n",applied.ic_file);
    load_ic_file_named(s,applied.ic_file);
  }
  if_needed_load_ext_options(s);
}

} // namespace xpp
