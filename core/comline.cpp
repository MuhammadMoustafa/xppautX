/* the command line of xppautX: its words read once into a CommandLine (comline.h) */
#include "comline.h"
#include "xpp_text.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <expected>

namespace xpp {

namespace {

/* the largest TCP port */
constexpr int MAX_PORT = 65535;

/* the options' ids (named, never numbers) */
enum class Opt { silent, newseed, outfile, anifile, statefile, noout, dfdraw, ncdraw,
                 equil, verbose, debug,
                 /* the program's own (xppautx_main.cpp) */
                 help, version, options_schema, convert, check, yes, no_open, browser, server, port };

struct CommandOption {
  std::string_view name;
  Opt id;
  bool argument;
  bool repeatable; /* given again adds (--silent, --noout, ...); otherwise a second one is an error */
  bool program;    /* one of the program's own switches, not a load's */
};

constexpr CommandOption my_cmd[] = {
  {"--silent", Opt::silent, false, true, false},
  {"--newseed", Opt::newseed, false, true, false},
  {"--outfile", Opt::outfile, true, false, false},
  {"--anifile", Opt::anifile, true, false, true}, /* --convert's second operand: the program's own, with --convert only */
  {"--statefile", Opt::statefile, true, false, true}, /* --convert's other second operand: an XPPAUT state file */
  {"--noout", Opt::noout, false, true, false},
  {"--dfdraw", Opt::dfdraw, true, false, false},
  {"--ncdraw", Opt::ncdraw, true, false, false},
  {"--equil", Opt::equil, true, false, false},
  {"--verbose", Opt::verbose, false, true, false},
  {"--debug", Opt::debug, false, true, false},
  {"--help", Opt::help, false, true, true},
  {"-h", Opt::help, false, true, true},
  {"--version", Opt::version, false, true, true},
  {"--options-schema", Opt::options_schema, false, true, true},
  {"--convert", Opt::convert, false, true, true},
  {"--check", Opt::check, false, true, true},
  {"--yes", Opt::yes, false, true, true},
  {"-y", Opt::yes, false, true, true},
  {"--no-open", Opt::no_open, false, true, true},
  {"--browser", Opt::browser, false, true, true},
  {"--server", Opt::server, false, true, true},
  {"--port", Opt::port, true, false, true},
};

const CommandOption *command_option(std::string_view word)
{
  for (const CommandOption &option : my_cmd)
    if (option.name == word) return &option;
  return nullptr;
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
  std::optional<Place> no_open_place;             /* --no-open's place: it needs --browser, checked after the pass */
  std::optional<Place> anifile_place;             /* --anifile's place: it needs --convert, checked after the pass */
  std::optional<Place> statefile_place;           /* --statefile's place: the same */
  const int count = static_cast<int>(cl.arguments.size());
  for (int i = 1; i < count; ++i) {
    const std::string_view word(cl.arguments[static_cast<std::size_t>(i)]);
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
    case Opt::convert: cl.convert = true; break;
    case Opt::check: cl.check = true; break;
    case Opt::yes: cl.yes = true; break;
    case Opt::no_open: cl.no_open = true; no_open_place = place; break;
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
    case Opt::newseed: cl.new_seed = true; break;
    case Opt::noout: cl.no_output = true; break;
    case Opt::verbose: cl.log_threshold = XPP_LOG_INFO; break;
    case Opt::debug: cl.log_threshold = XPP_LOG_DEBUG; break;
    case Opt::anifile: cl.ani_file = value; anifile_place = place; break;
    case Opt::statefile: cl.state_file = value; statefile_place = place; break;
    case Opt::outfile:
      cl.out_file = value;
      cl.out_place = xpp::format("{} {}", word, value);
      break;
    case Opt::equil: {
      int n = 0;
      if (const Result<> r = whole_number(n); !r) return std::unexpected(r.error());
      cl.equilibria = n;
      break;
    }
    case Opt::dfdraw: cl.options.emplace_back("DFDRAW", value); break;
    case Opt::ncdraw: cl.options.emplace_back("NCDRAW", value); break;
    }
  }
  if (anifile_place && !cl.convert)
    return fail("options", "--anifile needs --convert (it is the .ani that --convert writes into a .anix)", *anifile_place);
  if (statefile_place && !cl.convert)
    return fail("options", "--statefile needs --convert (it is the XPPAUT file --convert writes into the .odex)", *statefile_place);
  if (cl.state_file && cl.ani_file)
    return fail("options", "--statefile and --anifile both given: --convert takes one", *statefile_place);
  if (no_open_place && cl.interface != Interface::browser)
    return fail("options", "--no-open needs --browser (it prints the address, which only browser mode shows)", *no_open_place);
  return cl;
}

} // namespace xpp
