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

/* the largest TCP port */
constexpr int MAX_PORT = 65535;

/* the options' ids (named, never numbers) */
enum class Opt { silent, no_compile, newseed, setfile, parfile, outfile, icfile, internset, uset, rset, include,
                 qsets, qpars, qics, quiet, logfile, anifile, mkplot, plotfmt, noout, dfdraw, ncdraw, readset, with,
                 equil, verbose, debug,
                 /* the program's own (xppautx_main.cpp) */
                 help, version, options_schema, setx_schema, convert, check, yes, no_open, browser, server, port };

struct CommandOption {
  std::string_view name;
  Opt id;
  bool argument;
  bool repeatable; /* given again adds (--include, --uset, --rset); otherwise a second one is an error */
  bool program;    /* one of the program's own switches, not a load's */
};

constexpr CommandOption my_cmd[] = {
  {"--silent", Opt::silent, false, true, false},
  {"--no-compile", Opt::no_compile, false, true, false},
  {"--newseed", Opt::newseed, false, true, false},
  {"--setfile", Opt::setfile, true, false, false},
  {"--parfile", Opt::parfile, true, false, false},
  {"--outfile", Opt::outfile, true, false, false},
  {"--icfile", Opt::icfile, true, false, false},
  {"--internset", Opt::internset, true, false, false},
  {"--uset", Opt::uset, true, true, false},
  {"--rset", Opt::rset, true, true, false},
  {"--include", Opt::include, true, true, false},
  {"--qsets", Opt::qsets, false, true, false},
  {"--qpars", Opt::qpars, false, true, false},
  {"--qics", Opt::qics, false, true, false},
  {"--quiet", Opt::quiet, true, false, false},
  {"--logfile", Opt::logfile, true, false, false},
  {"--anifile", Opt::anifile, true, false, false},
  {"--mkplot", Opt::mkplot, false, true, false},
  {"--plotfmt", Opt::plotfmt, true, false, false},
  {"--noout", Opt::noout, false, true, false},
  {"--dfdraw", Opt::dfdraw, true, false, false},
  {"--ncdraw", Opt::ncdraw, true, false, false},
  {"--readset", Opt::readset, true, false, false},
  {"--with", Opt::with, true, false, false},
  {"--equil", Opt::equil, true, false, false},
  {"--verbose", Opt::verbose, false, true, false},
  {"--debug", Opt::debug, false, true, false},
  {"--help", Opt::help, false, true, true},
  {"-h", Opt::help, false, true, true},
  {"--version", Opt::version, false, true, true},
  {"--options-schema", Opt::options_schema, false, true, true},
  {"--setx-schema", Opt::setx_schema, false, true, true},
  {"--convert", Opt::convert, false, true, true},
  {"--check", Opt::check, false, true, true},
  {"--yes", Opt::yes, false, true, true},
  {"-y", Opt::yes, false, true, true},
  {"--no-open", Opt::no_open, false, true, true},
  {"--browser", Opt::browser, false, true, true},
  {"--web", Opt::browser, false, true, true},
  {"--server", Opt::server, false, true, true},
  {"--port", Opt::port, true, false, true},
};

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

} // namespace

/* the one pass over the command line: every option checked, its argument
   read and validated where it stands; the first fault is an error naming
   the option and its place */
Result<CommandLine> parse_command_line(std::vector<std::string> arguments)
{
  CommandLine cl;
  cl.arguments = std::move(arguments);
  std::vector<const CommandOption *> seen;
  std::optional<std::string_view> interface_word; /* the option that chose cl.interface */
  const int count = static_cast<int>(cl.arguments.size());
  for (int i = 1; i < count; ++i) {
    const std::string_view word(cl.arguments[static_cast<std::size_t>(i)]);
    const Place place{"command line", i, 0, std::string(word)};
#ifdef __APPLE__
    /* the process serial number an older macOS gave an app Finder started,
       which would be taken for a bad option */
    if (word.starts_with("-psn_")) continue;
#endif
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
        if (command_option(spelling))
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
      if (++i >= count) return fail("options", xpp::format("{} needs an argument", word), place);
      value = cl.arguments[static_cast<std::size_t>(i)];
    }
    if (!option->program) ++cl.load_options;
    const auto whole_number = [&](int &out) -> Result<> {
      if (!parse_int(value, out))
        return fail("options", xpp::format("{} needs a whole number, not \"{}\"", word, value), place);
      return {};
    };
    switch (option->id) {
    case Opt::help: cl.help = true; break;
    case Opt::version: cl.version = true; break;
    case Opt::options_schema: cl.options_schema = true; break;
    case Opt::setx_schema: cl.setx_schema = true; break;
    case Opt::convert: cl.convert = true; break;
    case Opt::check: cl.check = true; break;
    case Opt::yes: cl.yes = true; break;
    case Opt::no_open: cl.no_open = true; break;
    case Opt::browser:
    case Opt::server: {
      const Interface wanted = option->id == Opt::server ? Interface::server : Interface::browser;
      if (cl.interface && *cl.interface != wanted)
        return fail("options", xpp::format("{} given with {}: one interface", word, *interface_word), place);
      cl.interface = wanted;
      interface_word = word;
      break;
    }
    case Opt::port: {
      int n = 0;
      if (const Result<> r = whole_number(n); !r) return std::unexpected(r.error());
      if (n < 0 || n > MAX_PORT) return fail("options", xpp::format("{} must be 0 (any free port) to {}, not {}", word, MAX_PORT, n), place);
      cl.port = n;
      break;
    }
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

void apply_command_line(Session &s, CommandLine cl)
{
  s.got_file = 0;
  if (!cl.model_file.empty()) {
    s.model().this_file = cl.model_file;
    s.got_file = 1;
  }
  if (cl.new_seed) xpp::log(XPP_LOG_INFO, "Random number seed changed\n");
  if (cl.silent) batch_options.enabled = 1;
  if (cl.make_plot) s.integrator.make_plot_flag = 1;
  if (cl.no_compile) program.compile = false;
  if (cl.no_output) s.integrator.suppress_out = 1;
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
  /* --quiet and --logfile win over .xpprc and the model's @ lines */
  for (const auto &[name, value] : cl.options) {
    set_option(s, name, value, 1, NULL);
    if (name == "QUIET") log_settings.quiet_from_command_line = 1;
    else if (name == "LOGFILE") log_settings.file_from_command_line = 1;
  }
  s.model().command_line = std::move(cl);
}

static int if_needed_load_ext_options(xpp::Session &s)
{
  const CommandLine &cl = s.model().command_line;
  if (cl.external == ExternalOptions::none) return 1;
  if (cl.external == ExternalOptions::file) {
    const std::string &file = cl.external_value;
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
  if(const xpp::Result<> r=extract_action(s,("$ "+cl.external_value),xpp::Place{},true);!r){
    xpp::log(XPP_LOG_ERROR, "{}\n",r.error().text());
    return 0;
  }
  return 1;
}

int if_needed_select_sets(const xpp::Model &m)
{
  const CommandLine &cl = m.command_line;
  if (cl.use_sets.empty() && cl.skip_sets.empty() && !cl.use_all_sets) return 1;
  const std::vector<xpp::Model::InternalSet> &sets=m.intern_sets;
  std::vector<int> &use=batch_options.intern_set_use;
  int &used=batch_options.intern_sets_used;
  use.assign(sets.size(),1);
  for(std::size_t j=0;j<sets.size();j++)
  {
    const std::string &name=sets[j].name;
    use[j]=batch_options.use_intern_sets;
    used+=batch_options.use_intern_sets;
    if (is_set_name(cl.use_sets,name))
    {
      xpp::log(XPP_LOG_INFO, "Internal set {} was included\n",name);
      if (use[j]==0){used++;}
      use[j]=1;
    }
    if (is_set_name(cl.skip_sets,name))
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
  const CommandLine &cl = s.model().command_line;
  if(!cl.has_set_file) return 1;
  if(const xpp::Result<> r=import_settings(s,cl.set_file,true);!r)
  {
    xpp::log(XPP_LOG_ERROR, "{}\n",r.error().text());
    return 0;
  }
  return 1;
}

void load_command_line_values(xpp::Session &s)
{
  const CommandLine &cl = s.model().command_line;
  if_needed_load_set(s);
  if(cl.has_par_file){
    xpp::log(XPP_LOG_INFO, "Loading external parameter file: {}\n",cl.par_file);
    load_parameter_file_named(s,cl.par_file);
  }
  if(cl.has_ic_file){
    xpp::log(XPP_LOG_INFO, "Loading external initial condition file: {}\n",cl.ic_file);
    load_ic_file_named(s,cl.ic_file);
  }
  if_needed_load_ext_options(s);
}

} // namespace xpp
