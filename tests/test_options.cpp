/* The model's options as one table (model_options.h, W119): every row is
   found by its names; set from an .ode file's @ lines, it takes the
   value and counts as set; a load of a model that sets none gives every
   option of the Session its default again (nothing leaks from the model
   before); and every option a .set file holds comes back from one, those
   it does not hold left as they were; a value an option refuses stops the
   load at its line, in an .ode or an .odex, and the model before stays; an
   options file (an option line) is refused, @ lines of an include file apply.

   make test runs this from the top of the tree. */
#include "nullcline.h"
#include "load_args.h"
#include "xpptest.h"
#include "test_support.h"
#include "session.h"
#include "load_eqn.h"
#include "solver.h"
#include "model.h"
#include "command_def.h"
#include "userbut.h"
#include "model_options.h"
#include "session_settings.h"
#include "settings_text.h"
#include "model_text.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_prefs.h"
#include "browse.h"
#include "graphics.h"
#include "comline.h"
#include "line_style.h"
#include "odex.h"
#include "xpp_error.h"

#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <expected>
#include <optional>
#include <span>
#include <utility>

namespace {

const char plain_ode[] = "build/test_options_plain.ode";
const char options_ode[] = "build/test_options_set.ode";
const char set_file[] = "build/test_options.odex";

/* a model with three variables and no options */
const char model_text[] = "x'=-x\ny'=x\nz'=y\npar a=1,b=2\n";

bool write_file(const char *path, const std::string &text)
{
    /* a .ode opens only beside its own conversion or none: drop the
       .odex an earlier run left */
    const std::string_view file(path);
    if (file.ends_with(".ode")) xpp::files::remove(std::string(file) + "x");
    xpp::Writer w(path);
    return w && w.write(file.ends_with(".odex") ? versioned(text) : text) && w.commit();
}

/* the terminal's answers to the next questions: stdin is this text */
bool answer_with(const std::string &text)
{
    const char *answers = "build/test_options_answers.txt";
    const char *none = test_null_device();
    /* the file being read cannot be replaced on Windows: let go of it first */
    return std::freopen(none, "r", stdin) != nullptr && write_file(answers, text) && std::freopen(answers, "r", stdin) != nullptr;
}

xpp::Session *load(const char *path)
{
    char arg0[] = "test_options";
    std::string file(path);
    char *argv[] = {arg0, file.data(), nullptr};
    xpp::Loaded l = load_args(2, argv, 1);
    if (!l) printf("  %s does not load: %s\n", path, l.error().text().c_str());
    return l ? *l : nullptr;
}

/* the option's value as text, "" when the row has no member */
std::string value_of(xpp::Session &s, const xpp::OptionRow &r)
{
    if (r.real) return xpp::number(r.real(s));
    if (r.whole) return xpp::format("{}", r.whole(s));
    if (r.boolean) return r.boolean(s) ? "true" : "false";
    if (r.index) return xpp::format("{}", r.index.read(s));
    if (r.text) return r.text(s);
    if (r.show) return r.show(s);
    return "";
}

/* the member belongs to the Session (or a part of it), not to a process-wide
   setting such as the command line's batch_options: another Session's row
   reaches another member */
bool in_session(xpp::Session &s, const xpp::OptionRow &r)
{
    if (r.index) return true;
    xpp::Session other(s.model());
    const auto at = [&](xpp::Session &x) -> const void * {
        return r.real ? static_cast<void *>(&r.real(x))
             : r.whole ? static_cast<void *>(&r.whole(x))
             : r.boolean ? static_cast<void *>(&r.boolean(x))
             : r.text ? static_cast<void *>(&r.text(x)) : nullptr;
    };
    const void *p = at(s);
    return p && p != at(other);
}

/* the value an @ line gives row k, and what it reads back as; a number
   different for each row (so lo < hi in table order) */
struct Sample {
    std::string text, value;
};

Sample sample(std::size_t k, const xpp::OptionRow &r)
{
    /* options whose value is not just a number or a word */
    static const std::map<std::string_view, Sample> special = {
        {"interface.sliders", {"[]", ""}},
        {"interface.animation", {"test_options.anix", ""}},
        {"S1", {"x", ""}}, {"S2", {"y", ""}}, {"S3", {"z", ""}},
        {"SLO1", {"-1", ""}}, {"SLO2", {"-1", ""}}, {"SLO3", {"-1", ""}},
        {"SHI1", {"2", ""}}, {"SHI2", {"2", ""}}, {"SHI3", {"2", ""}},
        {"diagram.x_min", {"-0.5", "-0.5"}}, {"diagram.y_min", {"-0.5", "-0.5"}}, /* below their maxima, which come first in the table */
        {"continuation.min_step_size", {"0.001", "0.001"}}, {"continuation.step_size", {"0.02", "0.02"}}, {"continuation.max_step_size", {"0.5", "0.5"}},
        {"integration.method", {"e", "1"}},            /* the Euler method */
        {"poincare.map", {"m", "2"}},          /* max */
        {"poincare.variable", {"y", "2"}},
        {"plot.axes.kind", {"3", "5"}},            /* 3D */
        {"LT", {"-1", "-1"}},
        {"output.picture_file", {"svg", "build/test_options_set.svg"}}, /* XPPAUT's plotfmt: the model's name with the format */
        {"poincare.sign", {"-1", "0"}},         /* negative */
        {"plot.line.draw", {"points", "1"}}, {"plot.line.dash", {"dotted", "2"}}, {"plot.line.width", {"2", "2"}}, {"plot.line.marker", {"square", "1"}},
        {"plot.line.marker_size", {"4", "4"}},
        {"output.nullclines_file", {"nc.dat", "build/nc.dat"}}, {"output.after_run_file", {"after.dat", "build/after.dat"}}, {"output.data_file", {"out.dat", "build/out.dat"}}, {"continuation.special_points", {"cusp", "cusp"}},
        {"sweep.reset", {"n", "false"}},
        {"sweep.old_ic", {"n", "false"}},
        {"diagram.variable", {"y", "1"}},         /* autoX counts from x */
        {"analysis.histogram.column", {"y", "2"}},
        {"analysis.histogram2.column", {"z", "3"}},
        {"analysis.spectrum.column", {"y", "2"}},
        {"analysis.spectrum.column2", {"z", "3"}},
        {"BELL", {"1", ""}},
        {"TUTORIAL", {"1", ""}},
        {"GRADS", {"1", ""}},
        {"integration.torus.fold", {"x", "X"}},
        {"analysis.coupling", {"y=x", "X = 0\nY = x\nZ = 0"}},
        {"analysis.transpose.first_column", {"y", "y"}}, {"plot.array_plot.first_column", {"z", "z"}},
        {"integration.boundary_value.range_parameter", {"b", "b"}}, {"equilibria.range_parameter", {"b", "b"}},
        {"sweep.second_variable", {"z", "z"}}, {"integration.boundary_value.range_side", {"end", "1"}},
        {"sweep.cycle_color", {"true", "1"}}, {"equilibria.range_shoot", {"true", "1"}}, {"integration.boundary_value.range_cycle_color", {"true", "1"}},
        {"AUTOEVAL", {"1", ""}},
        {"integration.jacobian.kind", {"exact", "1"}},
        {"integration.subsystem_split", {"run", "1"}},
        {"integration.storage_precision", {"double", "1"}},
    };
    if (r.first_digit) return {r.name.ends_with('P') ? "y" : "1", ""}; /* xp2=y, xlo2=1 */
    if (const auto it = special.find(xpp::option_name(r)); it != special.end()) return it->second;
    if (r.real) {
        const double v = 0.25 + static_cast<double>(k) / 64;
        return {xpp::number(v), xpp::number(v)};
    }
    if (r.whole) return {"2", "2"};
    if (r.boolean) return {"yes", "true"};
    if (r.text) return {"abc", "abc"};
    return {"1", ""};
}

/* what the option model leaves out: a log file, a button, a list, and what an XPPAUT file says that no member of ours holds */
bool left_out(const xpp::OptionRow &r)
{
    /* run_set names a set of the model: a generated value never is one (W666), its own check is modecheck's */
    return xpp::option_name(r) == "interface.buttons" || r.name == "BUT" || xpp::option_name(r) == "output.run_set" || r.kind == xpp::OptionKind::sliders || r.kind == xpp::OptionKind::curves || r.expand
           || r.curve_field != xpp::CurveField::none || !r.dropped.empty() || r.in_statement;
}

/* whether a session file holds the option: the table's rows marked saved, and the
   torus period (its torus group) */
bool in_set_file(const xpp::OptionRow &r) { return r.saved || xpp::option_name(r) == "integration.torus.period"; }

std::string id(const xpp::OptionRow &r) { return xpp::option_name(r); }

} // namespace

/* the command line as main reads it: every option checked */
static xpp::Result<> check_args(int argc, char **argv)
{
    const xpp::Result<xpp::CommandLine> cl = xpp::parse_command_line(std::vector<std::string>(argv, argv + argc));
    if (!cl) return std::unexpected(cl.error());
    return {};
}

TEST_CASE("test_options")
{
    /* W132: all public names, set labels, legacy keys, case and persisted ids. */
    const auto owned_model = std::make_unique<xpp::Model>();
    xpp::Model &model = *owned_model;
    model.node = 2;
    const xpp::Place place{"pick.odex", 7, 1, "@ method=symplectic"};
    const std::string keys = "demragvbqsc582y";
    for (const auto &info : xpp::solvers()) {
        model.nkernel = info.traits.integral_history ? 1 : 0;
        for (const std::string &text : {std::string(info.name), std::string(info.set_label),
                                      xpp::upper_case(std::string(info.name)), std::string(1, keys[info.id]),
                                      xpp::upper_case(std::string(1, keys[info.id]))}) {
            const auto picked = xpp::pick_method(model, text, place);
            CHECK(picked && *picked == info.id);
        }
        const auto by_number = xpp::check_method(model, info.id, place);
        CHECK(by_number && *by_number == info.id);
    }
    model.nkernel = 0;
    for (const auto &alias : {std::pair{"disc", xpp::method::DISCRETE},
                             std::pair{"rk4", xpp::method::RK4},
                             std::pair{"qualrk4", xpp::method::RKQS},
                             std::pair{"modified euler", xpp::method::MOD_EULER},
                             std::pair{"backward euler", xpp::method::BACKEUL}}) {
        const auto picked = xpp::pick_method(model, alias.first, place);
        CHECK(picked && *picked == alias.second);
    }
    for (const int id : {-1, static_cast<int>(xpp::method::COUNT)}) CHECK(!xpp::check_method(model, id, place));
    for (const char *text : {"unknown", "rubbish", "", "#3", "volterra"}) {
        const auto picked = xpp::pick_method(model, text, place);
        CHECK(!picked);
        if (!picked) CHECK(picked.error().place.line == 7 && picked.error().place.file == "pick.odex");
    }
    model.node = 3;
    const auto refused = xpp::pick_method(model, "symplectic", place);
    CHECK(!refused && refused.error().what.find("even dimensions") != std::string::npos);
    model.nkernel = 1;
    CHECK(!xpp::pick_method(model, "euler", place));
    model.nkernel = 0;
    model.node = 2;
    model.nwiener = 1;
    for (const auto &info : xpp::solvers()) {
        model.nkernel = info.traits.integral_history ? 1 : 0;
        const auto wiener_picked = xpp::pick_method(model, info.name, place);
        if (info.traits.wiener) {
            CHECK(wiener_picked && *wiener_picked == info.id);
        } else {
            CHECK(!wiener_picked);
            if (!wiener_picked) CHECK(wiener_picked.error().what.find("Wiener") != std::string::npos);
        }
    }
    model.nkernel = 0;
    model.nwiener = 0;

    const std::span<const xpp::OptionRow> rows = xpp::option_rows();

    {
        char exe[] = "test_options";
        char silent[] = "--silent";
        char *argv[] = {exe, silent, nullptr};
        CHECK(check_args(2, argv).has_value());
        xpp::Session &session = xpp::client_session();
        xpp::batch_options.enabled = 0;
        xpp::Result<xpp::CommandLine> silent_line = xpp::parse_command_line({exe, silent});
        CHECK(silent_line.has_value());
        CHECK(silent_line->model_argument == -1);
        xpp::apply_command_line(session, std::move(*silent_line));
        CHECK(xpp::batch_options.enabled == 1);
        xpp::batch_options.enabled = 0;
        for (const char *name : {"silent", "anifile", "statefile", "outfile", "newseed", "version", "noout", "dfdraw", "ncdraw", "equil", "verbose", "debug"}) {
            std::string word = "-" + std::string(name);
            argv[1] = word.data();
            const xpp::Result<> r = check_args(2, argv);
            CHECK(!r);
            if (!r) {
                CHECK(r.error().what == word + " is -" + word);
                CHECK(r.error().place.file == "command line");
                CHECK(r.error().place.line == 1);
            }
        }
        for (const char *name : {"xorfix", "iconify", "allwin", "ee", "white", "bigfont", "smallfont", "forecolor", "backcolor", "backimage", "grads", "width", "height", "mwcolor", "dwcolor", "bell", "def"}) {
          for (const char *prefix : {"-", "--"}) {
            std::string word = prefix + std::string(name);
            argv[1] = word.data();
            const xpp::Result<> r = check_args(2, argv);
            CHECK(!r);
            if (!r) {
                CHECK(r.error().what == "no such option " + word);
                CHECK(r.error().place.file == "command line");
                CHECK(r.error().place.line == 1);
            }
          }
        }
        /* one pass: an argument is read and checked where it stands, and the error names the option */
        {
            const auto fault = [&](std::initializer_list<const char *> words) {
                std::vector<std::string> keep(words.begin(), words.end());
                std::vector<char *> v{exe};
                for (std::string &w : keep) v.push_back(w.data());
                v.push_back(nullptr);
                const xpp::Result<> r = check_args(static_cast<int>(v.size()) - 1, v.data());
                return r ? std::string() : r.error().what;
            };
            CHECK(fault({"--equil", "x"}) == "--equil needs a whole number, not \"x\"");
            /* W666: the set and query switches are unknown options */
            for (const char *removed : {"--internset", "--uset", "--rset", "--qsets", "--qpars", "--qics"})
                CHECK(fault({removed, "a"}) == std::string("no such option ") + removed);
            /* W665: an XPPAUT state file is converted once (--convert --statefile), never a switch of a run */
            for (const char *removed : {"--setfile", "--parfile", "--icfile"})
                CHECK(fault({removed, "a"}) == std::string("no such option ") + removed);
            CHECK(fault({"--statefile", "a.set"}) == "--statefile needs --convert (it is the XPPAUT file --convert writes into the .odex)");
            CHECK(fault({"--convert", "--statefile", "a.set", "model.ode"}).empty());
            CHECK(fault({"--convert", "--statefile", "a.set", "--anifile", "a.ani", "model.ode"}) == "--statefile and --anifile both given: --convert takes one");
            /* W667: the removed switches are unknown options, and --anifile is --convert's operand only */
            for (const char *removed : {"--readset", "--with", "--include", "--quiet", "--logfile"})
                CHECK(fault({removed, "a"}) == std::string("no such option ") + removed);
            CHECK(fault({"--anifile", "a.ani"}) == "--anifile needs --convert (it is the .ani that --convert writes into a .anix)");
            CHECK(fault({"--convert", "--anifile", "a.ani", "model.odex"}).empty());
            CHECK(fault({"--equil", "1", "model.ode"}).empty());
            CHECK(fault({"--equil"}) == "--equil needs an argument");
        }
        for (const char *word : {"--silently", "--nosuch", "--runnow", "--setfile"}) {
            std::string arg(word);
            argv[1] = arg.data();
            CHECK(!check_args(2, argv));
        }
    }

    for (const char *word : {"silent", "xorfix"}) {
        char exe[] = "test_options";
        std::string arg = "-" + std::string(word);
        char *argv[] = {exe, arg.data(), nullptr};
        const xpp::Loaded loaded = load_args(2, argv, 1);
        CHECK(!loaded);
        if (!loaded) CHECK(loaded.error().what.find(word) != std::string::npos);
    }

    /* Full registry names survive the .odex grammar and option reader. */
    for (const auto &info : xpp::solvers()) {
        if (info.traits.integral_history) continue; /* needs an integral model */
        CHECK(write_file("build/test_method_names.odex", xpp::format("x'=-x\ny'=x\noptions {{ integration: {{ method: {}, total_time: 1 }} }}\n", info.key)));
        xpp::Session *named = load("build/test_method_names.odex");
        CHECK(named && named->numerics->method == info.id && named->numerics->total_time == 1);
    }

    /* the limits and the nullclines' options: our own words in a .odex, XPPAUT's bound and ncdraw
       (each several of ours) in a .ode */
    CHECK(write_file("build/test_limits.odex", "x' = -x\noptions { integration: { state_limit: 7, stop_at_limit: false }, equilibria: { search_limit: 9 }, "
                                               "plot: { nullclines.draw: true }, output: { nullclines_file: \"nc.dat\" } }\n"));
    {
        const xpp::Session *l = load("build/test_limits.odex");
        CHECK(l && l->numerics->state_limit == 7 && !l->numerics->stop_at_limit && l->numerics->equilibria.search_limit == 9);
        CHECK(l && l->nullclines->draw && l->nullclines->file == "build/nc.dat");
    }
    for (const char *ncdraw : {"0", "1", "2"}) {
        CHECK(write_file("build/test_bound.ode", xpp::format("x'=-x\n@ bound=7 ncdraw={}\ndone\n", ncdraw)));
        const xpp::Session *l = load("build/test_bound.ode");
        CHECK(l && l->numerics->state_limit == 7 && l->numerics->stop_at_limit && l->numerics->equilibria.search_limit == 7);
        CHECK(l && l->nullclines->draw == (std::string_view(ncdraw) == "1") && l->nullclines->file == (std::string_view(ncdraw) == "2" ? "build/nullclines.dat" : ""));
    }
    /* XPPAUT's lt is the line style every new curve starts with: its line types; our own style is any */
    for (const int lt : {1, 0, -3}) {
        CHECK(write_file("build/test_lt.ode", xpp::format("x'=-x\n@ lt={}\ndone\n", lt)));
        const xpp::Session *l = load("build/test_lt.ode");
        CHECK(l && l->plot_settings->start_line_style == *xpp::line_style_of_xppaut(lt));
    }
    CHECK(write_file("build/test_lt.odex", "x' = -x\noptions { plot: { line.draw: points, line.marker_size: 4 } }\n"));
    {
        const xpp::Session *l = load("build/test_lt.odex");
        CHECK(l && l->plot_settings->start_line_style == *xpp::line_style_of_xppaut(-4));
    }
    CHECK(write_file("build/test_style.odex", "x' = -x\noptions { plot: { line.dash: dash_dot, line.width: 2.5, line.draw: both, line.marker: star, line.marker_size: 6 } }\n"));
    {
        const xpp::Session *l = load("build/test_style.odex");
        xpp::LineStyle want;
        want.draw = xpp::LineDraw::both;
        want.dash = xpp::LineDash::dash_dot;
        want.width = 2.5;
        want.marker = xpp::LineMarker::star;
        want.marker_size = 6;
        CHECK(l && l->plot_settings->start_line_style == want);
    }
    for (const char *bad : {"line.dash: dotted dashed", "line.width: 0", "line.marker: 5", "line.marker_size: -1", "line.draw: 1"}) {
        CHECK(write_file("build/test_bad_choice.odex", xpp::format("x' = -x\noptions {{ plot: {{ {} }} }}\n", bad)));
        char arg0[] = "test_options";
        char file[] = "build/test_bad_choice.odex";
        char *argv[] = {arg0, file, nullptr};
        CHECK(!load_args(2, argv, 1).has_value());
    }

    /* a number or a yes for a choice is no word of ours */
    for (const char *bad : {"poincare: { sign: 1 }", "poincare: { stop: 1 }", "integration: { stop_at_limit: yes }", "integration: { method: 3 }"}) {
        CHECK(write_file("build/test_bad_choice.odex", xpp::format("x' = -x\noptions {{ {} }}\n", bad)));
        char arg0[] = "test_options";
        char file[] = "build/test_bad_choice.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = load_args(2, argv, 1);
        CHECK(!l.has_value());
        if (!l) CHECK(l.error().place.line == 3);
    }

    /* each name finds its row */
    for (const xpp::OptionRow &r : rows) {
        const std::string own = xpp::option_name(r);
        for (std::string_view n : {std::string_view(own), r.xppaut_names[0], r.xppaut_names[1]}) {
            if (n.empty()) continue;
            int index = -1;
            std::string name(n);
            if (r.first_digit) name += r.last_digit;
            const xpp::OptionRow *found = xpp::find_option(name, index, true);
            CHECK(found == &r);
            if (found != &r) printf("  %s finds another row\n", name.c_str());
            if (r.first_digit) CHECK(index == r.last_digit - '0');
        }
        if (!r.key.empty()) CHECK(xpp::numerics_option(r.key) == &r);
        /* a numbered row is its name and a digit; a once row is the only one of its name (once_option finds it) */
        if (r.once) {
            CHECK(&xpp::once_option(own) == &r);
            for (const xpp::OptionRow &o : rows)
                CHECK(&o == &r || !o.once || xpp::option_name(o) != own);
        }
    }
    /* orders and the new ranges: one rule for every source */
    {
        CHECK(!xpp::order_problem("xlo", 0, "xhi", 1, true) && !xpp::order_problem("min_step", 1, "max_step", 1, false));
        const auto bad = xpp::order_problem("xlo", 3, "xhi", 1, true);
        CHECK(bad && *bad == "xlo (3) must be below xhi (1)");
        CHECK(xpp::order_problem("min_step", 2, "max_step", 1, false));
        CHECK(xpp::rule_problem(xpp::OptionRule::whole_at_least_two, 1) && !xpp::rule_problem(xpp::OptionRule::whole_at_least_two, 2));
    }
    /* the longest name wins: histlo2 is not histlo, dtmin not dt */
    {
        int index = 0;
        CHECK(xpp::option_name(*xpp::find_option("HISTLO2", index, true)) == "analysis.histogram2.low");
        CHECK(xpp::option_name(*xpp::find_option("DTMINIMUM", index, true)) == "integration.step.min");
        CHECK(xpp::option_name(*xpp::find_option("METHOD", index, true)) == "integration.method" && xpp::option_name(*xpp::find_option("METH", index, true)) == "integration.method");
        CHECK(xpp::find_option("XP9", index, true)->name == "XP");
        CHECK(xpp::find_option("XP3", index, true)->first_digit == '2' && index == 3);
        CHECK(xpp::find_option("NOSUCH", index, true) == nullptr);
        CHECK(xpp::option_name(*xpp::find_option("NOUT", index, true)) == "integration.store_every" && xpp::find_option("NJMP", index, true) == xpp::find_option("NOUT", index, true));
        CHECK(xpp::find_option("NOUT", index, false) == nullptr && xpp::odex_option_key("NJMP") == "store_every" && xpp::odex_option_key("dt") == "dt"
               && xpp::odex_option_key("DTMIN") == "step.min" && xpp::odex_option_key("meth") == "method" && xpp::odex_option_key("method") == "method");
        /* XPPAUT's plotted variables are the `curves` list's on import only: no key of theirs, nor a name no group has */
        CHECK(xpp::odex_option_key("curves") == "curves" && xpp::odex_option_key("xplot").empty() && xpp::odex_option_key("XP3").empty()
               && xpp::odex_option_key("bell").empty() && xpp::odex_option_key("nosuch").empty());
        CHECK(xpp::find_odex_option("plot", "curves", index) && !xpp::find_odex_option("plot", "curves.x", index) && !xpp::find_odex_option("plot", "xplot", index)
               && !xpp::find_odex_option("integration", "total", index) && !xpp::find_odex_option("integration", "storage_rows", index));
        /* what became after_run's choices, and the sweeps' one graph, are XPPAUT's names only */
        CHECK(xpp::find_option("POSTPROCESS", index, true)->odex == "after_run" && !xpp::find_option("POSTPROCESS", index, false)
               && xpp::find_option("STOCH", index, true)->expand && xpp::find_option("MAXSTOR", index, true)->odex.empty()
               && !xpp::find_option("MULTIWIN", index, true)->dropped.empty() && !xpp::find_option("XLO3", index, true)->dropped.empty());
        /* a key is its group's: the same word in another group is another row, a top-level row has no group */
        CHECK(xpp::option_name(*xpp::find_odex_option("poincare", "variable", index)) == "poincare.variable" && xpp::option_name(*xpp::find_odex_option("sweep", "variable", index)) == "sweep.variable"
               && !xpp::find_odex_option("plot", "variable", index) && xpp::find_odex_option("", "animation", index) && !xpp::find_odex_option("interface", "animation", index));
    }

    /* W660: an expanding row declares the rows it sets, and that list is what its expansion sets; each target's help says where its value
       comes from in a .ode, derived from the table, and a row no .ode can set says nothing about it */
    {
        const std::map<std::string_view, std::vector<std::string_view>> samples = {{"STOCH", {"0", "1", "2"}}, {"BOUND", {"5"}}, {"LT", {"1", "0", "-2"}}, {"NCDRAW", {"0", "1", "2"}}};
        const auto row_named = [&](std::string_view name) -> const xpp::OptionRow * {
            for (const xpp::OptionRow &r : rows)
                if (!r.odex.empty() && xpp::option_name(r) == name) return &r;
            return nullptr;
        };
        for (const xpp::OptionRow &r : rows) {
            if (!r.expand) continue;
            const auto given = samples.find(r.name);
            std::set<std::string> got, declared;
            for (const std::string_view value : given == samples.end() ? std::vector<std::string_view>{"1"} : given->second) {
                const xpp::ForeignOptions ours = r.expand(value);
                CHECK(!ours.why);
                for (const auto &item : ours.items) got.insert(item.first);
            }
            for (const std::string_view target : r.sets) declared.insert(std::string(target));
            CHECK(got == declared);
            if (got != declared) printf("  %s: sets %zu rows, expands to %zu\n", std::string(r.name).c_str(), declared.size(), got.size());
            for (const std::string_view target : r.sets) {
                const xpp::OptionRow *row = row_named(target);
                CHECK(row && xpp::option_help(*row).find("From an XPPAUT .ode: ") != std::string::npos && xpp::option_help(*row).find(r.name) != std::string::npos);
            }
        }
        const xpp::OptionRow *limit = row_named("integration.state_limit"), *total = row_named("integration.total_time"), *on = row_named("integration.torus.on");
        CHECK(limit && total && on);
        if (limit && total && on) {
            CHECK(xpp::option_help(*limit) == std::string(limit->help) + " From an XPPAUT .ode: BOUND (which also sets equilibria.search_limit): the one value is both limits.");
            CHECK(xpp::option_help(*total) == std::string(total->help) + " From an XPPAUT .ode: TOTAL.");
            CHECK(xpp::option_help(*on) == on->help);
        }
    }

    /* the defaults a model with no options gets */
    CHECK(write_file(plain_ode, std::string(model_text) + "done\n"));
    xpp::Session *s = load(plain_ode);
    CHECK(s != nullptr);
    /* a model's own set or action takes the new names only; the command line's settings take XPPAUT's too */
    if (s) {
        CHECK(xpp::option_problem(*s, "total", "5", false).has_value());
        CHECK(!xpp::option_problem(*s, "total", "5", true).has_value());
        CHECK(!xpp::option_problem(*s, "integration.total_time", "5", false).has_value() && xpp::option_problem(*s, "total_time", "5", false).has_value());
        CHECK(xpp::option_problem(*s, "meth", "e", false).has_value() && !xpp::option_problem(*s, "integration.method", "euler", false).has_value());
        /* W663: a button is a label and a registry command in our own syntax; XPPAUT's BUT (name:keys) is read on import only, a sequence no command of ours runs is dropped there */
        CHECK(!xpp::option_problem(*s, "interface.buttons", "[ { label: \"Go\", command: run.initial } ]", false).has_value());
        CHECK(xpp::option_problem(*s, "interface.buttons", "[ { label: \"Go\", command: nosuch.command } ]", false).has_value());
        CHECK(xpp::option_problem(*s, "interface.buttons", "[ { label: \"\", command: run.initial } ]", false).has_value());
        CHECK(xpp::option_problem(*s, "interface.buttons", "[ { label: \"Go\", command: run.initial }, { label: \"Go\", command: run.last } ]", false).has_value());
        /* W662 review: a sequence no command of ours runs is refused, never accepted silently */
        CHECK(!xpp::option_problem(*s, "but", "quit:fq", true).has_value() && xpp::option_problem(*s, "but", "pdf:gp", true).has_value());
        CHECK(xpp::option_problem(*s, "but", "nokeys", true).has_value());
        /* every key sequence XPPAUT's buttons took (one to three letters) that has a command of ours has a registered one */
        std::size_t mapped = 0;
        std::string keys;
        for (char a = 'a'; a <= 'z'; ++a)
            for (int length = 1; length <= 3; ++length)
                for (char b = (length > 1 ? 'a' : 'z'); b <= 'z'; ++b)
                    for (char c = (length > 2 ? 'a' : 'z'); c <= 'z'; ++c) {
                        keys = std::string(1, a) + (length > 1 ? std::string(1, b) : std::string()) + (length > 2 ? std::string(1, c) : std::string());
                        const std::string_view command = xpp::xppaut_button_command(keys);
                        if (command.empty()) continue;
                        ++mapped;
                        CHECK(xpp::command::find(command));
                    }
        CHECK(mapped > 30);
        CHECK(!xpp::option_problem(*s, "integration.method", "euler", false).has_value());
        /* an XPPAUT choice's letter, number or yes/no is a foreign file's, never our own */
        CHECK(!xpp::option_problem(*s, "meth", "e", true).has_value() && xpp::option_problem(*s, "integration.method", "1", false).has_value());
        CHECK(xpp::option_problem(*s, "poincare.sign", "1", false).has_value() && !xpp::option_problem(*s, "poincare.sign", "positive", false).has_value());
        CHECK(!xpp::option_problem(*s, "poisgn", "1", true).has_value() && !xpp::option_problem(*s, "poistop", "1", true).has_value());
        CHECK(xpp::option_problem(*s, "poincare.stop", "1", false).has_value() && xpp::option_problem(*s, "sweep.reset", "yes", false).has_value());
        CHECK(!xpp::option_problem(*s, "sweep.reset", "true", false).has_value() && !xpp::option_problem(*s, "rangereset", "y", true).has_value());
    }
    if (!s) TEST_REPORT("options");
    std::vector<std::string> defaults;
    for (const xpp::OptionRow &r : rows) defaults.push_back(value_of(*s, r));
    for (const xpp::OptionRow &r : rows)
        if (r.once && !r.reset && xpp::option_name(r) != "plot.colormap") CHECK(!s->options_set.has(r)); /* an .ode's conversion sets its colormap, jet */

    /* every option set from @ lines, the last row's first (xmin sets the
       2D window's xlo too; xlo's own line after it wins) */
    CHECK(write_file("build/test_options.anix", "line { x1: 0, y1: 0, x2: 1, y2: 1 }\n"));
    std::string text(model_text);
    for (std::size_t k = rows.size(); k-- > 0;) {
        const xpp::OptionRow &r = rows[k];
        if (left_out(r)) continue;
        std::string name = xpp::option_name(r);
        if (r.first_digit) name += r.first_digit;
        text += xpp::format("@ {}={}\n", name, sample(k, r).text);
    }
    text += "done\n";
    CHECK(write_file(options_ode, text));
    s = load(options_ode);
    CHECK(s != nullptr);
    if (!s) TEST_REPORT("options");
    for (std::size_t k = 0; k < rows.size(); k++) {
        const xpp::OptionRow &r = rows[k];
        if (left_out(r)) continue;
        const Sample want = sample(k, r);
        if (!want.value.empty()) {
            const std::string got = value_of(*s, r);
            CHECK(got == want.value);
            if (got != want.value) printf("  %s: %s, not %s\n", std::string(id(r)).c_str(), got.c_str(), want.value.c_str());
        }
        /* Imported S1-S3 slots become the one native preset-list row. */
        if (r.once && !(r.group.empty() && r.parse && r.name.starts_with("S"))) CHECK(s->options_set.has(r));
    }

    CHECK(s->sliders.size() == 3 && s->sliders[0].var == "x" && s->sliders[2].var == "z");
    CHECK(s->sliders[0].lo == -1 && s->sliders[2].hi == 2);

    /* a session state brings back what it holds */
    init_data(*s);
    init_all_graph(*s);
    for (std::size_t k = 0; k < rows.size(); k++) {
        const xpp::OptionRow &r = rows[k];
        if (xpp::option_name(r).empty() && r.key.empty()) continue;
        const bool held = in_set_file(r);
        CHECK(!held || r.show || in_session(*s, r)); /* a list row has no member, its text is its value */
        if (!r.key.empty()) CHECK(held); /* every numerics setting the menu edits is in a session file */
    }
    const xpp::Result<std::string> own = xpp::settings_text(*s, xpp::capture_settings(*s, xpp::SettingsUse::session));
    CHECK(own.has_value());
    if (!own) TEST_REPORT("options");
    CHECK(settings_text::read(*s, set_file, *own).has_value());
    std::vector<std::string> written;
    for (const xpp::OptionRow &r : rows) {
        written.push_back(value_of(*s, r));
        if (!in_session(*s, r)) continue;
        if (r.real) r.real(*s) += 1;
        else if (r.whole && r.saved && r.kind == xpp::OptionKind::choice) r.whole(*s) = (r.whole(*s) + 1) % static_cast<int>(xpp::option_choices(r).size());
        else if (r.whole) r.whole(*s) += 1;
        else if (r.boolean) r.boolean(*s) = !r.boolean(*s);
        else if (r.index) r.index.write(*s, (r.index.read(*s) + 1) % static_cast<int>(xpp::option_choices(r).size()));
        else if (r.text) r.text(*s) = "xyz";
    }
    std::vector<std::string> changed;
    for (const xpp::OptionRow &r : rows) changed.push_back(value_of(*s, r));
    const xpp::Result<xpp::Settings> back = settings_text::read(*s, set_file, *own);
    CHECK(back.has_value());
    if (back) xpp::apply_settings(*s, *back, false);
    for (std::size_t k = 0; k < rows.size(); k++) {
        const xpp::OptionRow &r = rows[k];
        if (!in_session(*s, r)) continue;
        const bool restored = in_set_file(r) && r.use != xpp::OptionUse::integral_history && r.key != "delay"; /* a model without delays or an integral method has neither setting */
        const std::string want = restored ? written[k] : changed[k];
        const std::string got = value_of(*s, r);
        CHECK(got == want);
        if (got != want) printf("  .set %s: %s, not %s\n", std::string(id(r)).c_str(), got.c_str(), want.c_str());
    }

    /* a model with no options: every option of the Session its default
       again */
    s = load(plain_ode);
    CHECK(s != nullptr);
    if (!s) TEST_REPORT("options");
    for (std::size_t k = 0; k < rows.size(); k++) {
        const xpp::OptionRow &r = rows[k];
        if (!in_session(*s, r)) continue;
        const std::string got = value_of(*s, r);
        CHECK(got == defaults[k]);
        if (got != defaults[k]) printf("  reset %s: %s, not %s\n", std::string(id(r)).c_str(), got.c_str(), defaults[k].c_str());
    }

    /* a value an option refuses stops the load at its line, the value
       named; the model before stays */
    const char bad_ode[] = "build/test_options_bad.ode";
    const char bad_odex[] = "build/test_options_bad.odex";
    for (const char *bad : {"dt=abc", "xp=nosuch", "meth=k", "nout=2.5", "rangereset=maybe",
                            "axes=4", "seed=-1", "lt=3", "histcol=nosuch", "xlo2=1e",
                            /* a number the setting's rule refuses (OptionRule) */
                            "nout=0", "njmp=0", "dt=0", "nmesh=0", "bound=-1", "delay=-1", "tol=0"}) {
        CHECK(write_file(bad_ode, std::string(model_text) + "@ total=5\n@ " + bad + "\ndone\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.ode";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = load_args(2, argv, 1);
        CHECK(!l.has_value());
        CHECK(&xpp::client_session() == s);
        if (l) continue;
        const std::string value(std::string_view(bad).substr(std::string_view(bad).find('=') + 1));
        CHECK(l.error().place.file == bad_ode);
        CHECK(l.error().place.line == 6);
        CHECK(l.error().what.find(value) != std::string::npos);
        if (l.error().place.line != 6 || l.error().what.find(value) == std::string::npos)
            printf("  %s: %s\n", bad, l.error().text().c_str());
    }
    /* Each allocation limit is checked at the original model line, before allocation. */
    for (const xpp::OptionRow &row : xpp::option_rows()) {
        if (!row.maximum || row.kind == xpp::OptionKind::curves) continue; /* a list's length is checked where it is read (curves) */
        CHECK(!xpp::option_problem(*s, xpp::option_name(row), std::to_string(row.maximum), false));
        const std::string value = std::to_string(row.maximum + 1);
        for (bool native : {false, true}) {
            if (native && row.odex.empty()) continue; /* an XPPAUT option has no key of ours */
            const char *path = native ? bad_odex : bad_ode;
            const std::string text = native ? "x' = -x\noptions { " + std::string(row.group) + ": { " + xpp::odex_key(row, 0) + ": " + value + " } }\n"
                                            : "x'=-x\n@ " + xpp::option_name(row) + "=" + value + "\ndone\n";
            CHECK(write_file(path, text));
            char arg0[] = "test_options";
            std::string file(path);
            char *argv[] = {arg0, file.data(), nullptr};
            const xpp::Loaded refused = load_args(2, argv, 1);
            CHECK(!refused);
            if (!refused) {
                CHECK(refused.error().place.file == path && refused.error().place.line == (native ? 3 : 2));
                CHECK(refused.error().what.find(value) != std::string::npos);
            }
            CHECK(&xpp::client_session() == s);
            CHECK(xpp::option_value_schema(row).find("\"maximum\": " + std::to_string(row.maximum)) != std::string::npos);
        }
    }
    for (const char *range : {"x[0..2000000000]'=-x[j]\n", "x[2000000000..0]'=-x[j]\n", "x[0..2000000000](0)=0\n"}) {
        CHECK(write_file(bad_ode, std::string(range) + "done\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.ode";
        char *argv[] = {arg0, file, nullptr};
        const auto refused = load_args(2, argv, 1);
        CHECK(!refused);
        if (!refused) {
            CHECK(refused.error().place.file == bad_ode && refused.error().place.line == 1);
            CHECK(refused.error().what.find("maximum 100000") != std::string::npos);
        }
    }
    /* Continuation rules are shared with the form and session reader; a bad
       complete model leaves the previous client session untouched. */
    for (const std::string setting : {"dsmin: -0.001", "dsmax: 0", "ds: 0", "epsl: 0", "epsu: -1", "epss: 0",
                                     "ncol: 1", "npr: 0", "dsmin: 0.2, dsmax: 0.1", "ds: 0.8, dsmax: 0.5"}) {
        CHECK(write_file(bad_odex, "x' = -x\noptions { continuation: { " + setting + " } }\n"));
        char arg0[] = "test_options", file[] = "build/test_options_bad.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded loaded = load_args(2, argv, 1);
        CHECK(!loaded);
        CHECK(&xpp::client_session() == s);
        if (!loaded) CHECK(loaded.error().place.file == bad_odex && loaded.error().place.line == 3);
    }
    /* W655: the eight colors are the user's preferences, so a .odex naming one is refused at its line */
    for (const std::string key : {"plot: { nullclines.x_color", "plot: { nullclines.y_color", "plot: { manifolds.stable_color", "plot: { manifolds.unstable_color",
                                  "diagram: { colors.stable_equilibrium", "diagram: { colors.unstable_equilibrium", "diagram: { colors.stable_periodic",
                                  "diagram: { colors.unstable_periodic"}) {
        CHECK(write_file(bad_odex, "x' = -x\noptions { " + key + ": \"#ff0000\" } }\n"));
        char arg0[] = "test_options", file[] = "build/test_options_bad.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded loaded = load_args(2, argv, 1);
        CHECK(!loaded);
        if (!loaded) CHECK(loaded.error().place.line == 3 && loaded.error().what.find("is not an option") != std::string::npos);
    }
    /* XPPAUT's numeric findext type is an import spelling, never native text. */
    for (const std::string type : {"-1", "0", "1", "sideways"}) {
        CHECK(write_file(bad_odex,"x'=-x\nnetwork ext = findext("+type+", 1, 1, x)\n"));
        char arg0[]="test_options", file[]="build/test_options_bad.odex";
        char *argv[]={arg0,file,nullptr};
        const auto loaded=load_args(2,argv,1);
        CHECK(!loaded);
        if(!loaded) CHECK(loaded.error().place.line==3);
    }
    /* XPPAUT's names for the renamed options are a .ode's: a .odex refuses them
       at their line */
    for (const char *old_name : {"nout", "njmp", "total", "t0", "trans", "nmesh", "newt_iter", "newt_tol", "jac_eps", "bound", "ncdraw", "meth", "tol", "dtmin", "dtmax", "atol", "maxstor", "tor_per", "poimap", "poivar", "poisgn", "poistop", "poipln"}) {
        CHECK(write_file(bad_odex, std::string("par a = 1\nx' = -a*x\noptions { integration: { ") + old_name + ": 2 } }\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = load_args(2, argv, 1);
        CHECK(!l.has_value());
        if (l) continue;
        CHECK(l.error().place.file == bad_odex);
        CHECK(l.error().place.line == 4);
        CHECK(l.error().what.find(old_name) != std::string::npos);
    }
    CHECK(xpp::client_session().model().this_file == xpp::odex::odex_name(plain_ode));

    /* XPPAUT's options file is not supported: an option line stops the load
       at its line, saying to write @ lines; the model before stays */
    for (const char *word : {"option", "options"}) {
        CHECK(write_file(bad_ode, std::string(model_text) + word + " foo.opt\ndone\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.ode";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = load_args(2, argv, 1);
        CHECK(!l.has_value());
        CHECK(&xpp::client_session() == s);
        if (l) continue;
        CHECK(l.error().place.file == bad_ode);
        CHECK(l.error().place.line == 5);
        CHECK(l.error().what.find("foo.opt") != std::string::npos);
        CHECK(l.error().what.find("@ lines") != std::string::npos);
        if (l.error().place.line != 5) printf("  %s: %s\n", word, l.error().text().c_str());
    }

    /* an imported file's options are not imported (W282): the options belong to the model that runs */
    {
        const char inc_file[] = "build/test_options_imp.odex";
        CHECK(write_file(inc_file, "par b = 2\noptions { integration: { total_time: 7, dt: 0.25 } }\n"));
        CHECK(write_file(bad_odex, "par a = 1\nx' = -a*x\nimport \"test_options_imp.odex\"\noptions { integration: { dt: 0.5 } }\n"));
        xpp::Session *t = load(bad_odex);
        CHECK(t != nullptr);
        if (t) {
            int index = 0;
            CHECK(value_of(*t, *xpp::find_option("TOTAL", index, true)) == xpp::number(20.0));
            CHECK(value_of(*t, *xpp::find_option("DT", index, true)) == xpp::number(0.5));
        }
        remove(inc_file);
    }

    /* an .ode's #include opts.inc (the name as written, #done last) is found next to the model, not in the working folder (the tree's top) */
    {
        remove(bad_odex);
        const char inc_file[] = "build/test_options_inc.inc";
        CHECK(write_file(inc_file, "@ total=7\n@ dt=0.25\n#done\n"));
        CHECK(write_file(bad_ode, std::string(model_text) + "#include test_options_inc.inc\ndone\n"));
        xpp::Session *t = load(bad_ode);
        CHECK(t != nullptr);
        if (t) {
            int index = 0;
            CHECK(value_of(*t, *xpp::find_option("TOTAL", index, true)) == xpp::number(7.0));
            CHECK(value_of(*t, *xpp::find_option("DT", index, true)) == xpp::number(0.25));
        }
        remove(inc_file);
    }

    /* an include file that cannot be read stops the load: the #include
       line's own (the model's file and line) */
    {
        const xpp::Session *before = &xpp::client_session();
        CHECK(write_file(bad_ode, std::string(model_text) + "#include nosuch.inc\ndone\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.ode";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = load_args(2, argv, 1);
        CHECK(!l.has_value());
        CHECK(&xpp::client_session() == before);
        if (!l) {
            CHECK(l.error().place.file == bad_ode);
            CHECK(l.error().place.line == 5);
            CHECK(l.error().what.find("nosuch.inc") != std::string::npos);
            if (l.error().place.line != 5) printf("  include: %s\n", l.error().text().c_str());
        }
    }

    /* outside the folder is a question (below): these answer it from a config
       folder of their own, with nothing typed */
    xpp::TempDir config;
    CHECK(!config.path().empty() && set_config_dir((config.path() + "/config").c_str()));
    CHECK(answer_with(""));

    /* OUTPUT names the batch data file, as XPPAUT's batchout: inside the
       model's folder (here build/), in an .ode and in an .odex; a path out of
       it is refused at its line */
    for (const bool odex : {false, true}) {
        const char *file_name = odex ? bad_odex : bad_ode;
        struct Case {
            const char *value, *written;
        };
        for (const Case c : {Case{"out.dat", "build/out.dat"}, Case{"sub/o.dat", "build/sub/o.dat"},
                             Case{"../simulations/xpp.dat", ""}, Case{"/tmp/x.dat", ""}, Case{"sub/../../x.dat", ""}}) {
            const std::string text = odex ? xpp::format("x' = -x\noptions {{ output: {{ data_file: \"{}\" }} }}\n", c.value)
                                          : std::string(model_text) + "@ total=5\n@ output=" + c.value + "\ndone\n";
            CHECK(write_file(file_name, text));
            char arg0[] = "test_options";
            char arg1[64];
            std::snprintf(arg1, sizeof arg1, "%s", file_name);
            char *argv[] = {arg0, arg1, nullptr};
            const xpp::Loaded l = load_args(2, argv, 1);
            if (*c.written) {
                CHECK(l.has_value());
                CHECK((*l)->batch_out_file == c.written);
                continue;
            }
            CHECK(!l.has_value());
            if (l) continue;
            CHECK(l.error().place.file == file_name);
            CHECK(l.error().place.line == (odex ? 3 : 6));
            CHECK(l.error().what.find(c.value) != std::string::npos);
            if (l.error().place.line != (odex ? 3 : 6)) printf("  output %s: %s\n", c.value, l.error().text().c_str());
        }
    }

    /* a path outside the folder is a question (W286), asked once, in a
       config folder of its own: no answer refuses at the line; -y allows
       it once and says so; "o" allows this load; "a" is kept for this model
       file and this path, so the next load does not ask, and another path asks */
    {
        const auto load_output = [&](const char *path) {
            CHECK(write_file(bad_odex, std::string("x' = -x\noptions { output: { data_file: \"") + path + "\" } }\n"));
            char arg0[] = "test_options";
            char arg1[64];
            std::snprintf(arg1, sizeof arg1, "%s", bad_odex);
            char *argv[] = {arg0, arg1, nullptr};
            return load_args(2, argv, 1);
        };
        const char outside[] = "../simulations/out.dat", other[] = "../simulations/other.dat";
        CHECK(answer_with(""));
        xpp::Loaded l = load_output(outside);
        CHECK(!l.has_value());
        if (!l) {
            CHECK(l.error().place.file == bad_odex && l.error().place.line == 3);
            CHECK(l.error().what.find("refused") != std::string::npos);
        }
        CHECK(answer_with("r\n"));
        CHECK(!load_output(outside).has_value());
        CHECK(answer_with("nonsense\n"));
        CHECK(!load_output(outside).has_value());
        xpp::batch_options.yes = true;
        l = load_output(outside);
        xpp::batch_options.yes = false;
        CHECK(l.has_value() && (*l)->batch_out_file == std::string("build/") + outside);
        CHECK(answer_with("o\n"));
        CHECK(load_output(outside).has_value());
        CHECK(answer_with("")); /* "once" kept nothing: asked again, no answer */
        CHECK(!load_output(outside).has_value());
        CHECK(answer_with("a\n"));
        CHECK(load_output(outside).has_value());
        CHECK(answer_with(""));
        CHECK(load_output(outside).has_value()); /* kept: no question */
        CHECK(!load_output(other).has_value());  /* another path asks again */
        CHECK(xpp::files::exists(xpp::prefs::prefs_path()));
        set_config_dir(nullptr);
    }

    remove(bad_ode);
    remove(bad_odex);
    remove(plain_ode);
    remove(options_ode);
    remove(set_file);
    remove("build/test_options.anix");
    TEST_REPORT("options");
}
