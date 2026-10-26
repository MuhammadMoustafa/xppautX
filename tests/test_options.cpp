/* The model's options as one table (model_options.h, W119): every row is
   found by its names; set from an .ode file's @ lines, it takes the
   value and counts as set; a load of a model that sets none gives every
   option of the Session its default again (nothing leaks from the model
   before); and every option a .set file holds comes back from one, those
   it does not hold left as they were; a value an option refuses stops the
   load at its line, in an .ode or an .odex, and the model before stays; an
   options file (an option line) is refused, @ lines of an include file apply.

   make test runs this from the top of the tree. */
#include "xpptest.h"
#include "session.h"
#include "solver.h"
#include "model.h"
#include "numerics_settings.h"
#include "model_options.h"
#include "lunch-new.h"
#include "setx_file.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_prefs.h"
#include "browse.h"
#include "graphics.h"
#include "comline.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

const char plain_ode[] = "build/test_options_plain.ode";
const char options_ode[] = "build/test_options_set.ode";
const char set_file[] = "build/test_options.setx";

/* a model with three variables and no options */
const char model_text[] = "x'=-x\ny'=x\nz'=y\npar a=1,b=2\n";

bool write_file(const char *path, const std::string &text)
{
    /* a .ode opens only beside its own conversion or none: drop the
       .odex an earlier run left */
    const std::string_view file(path);
    if (file.ends_with(".ode")) xpp::files::remove(std::string(file) + "x");
    xpp::Writer w(path);
    return w && w.write(text) && w.commit();
}

/* the terminal's answers to the next questions: stdin is this text */
bool answer_with(const std::string &text)
{
    const char *answers = "build/test_options_answers.txt";
#ifdef _WIN32
    const char *none = "NUL";
#else
    const char *none = "/dev/null";
#endif
    /* the file being read cannot be replaced on Windows: let go of it first */
    return std::freopen(none, "r", stdin) != nullptr && write_file(answers, text) && std::freopen(answers, "r", stdin) != nullptr;
}

bool set_config_dir(const char *dir)
{
#ifdef _WIN32
    return _putenv_s(xpp::files::CONFIG_DIR_ENV, dir ? dir : "") == 0;
#else
    return dir ? setenv(xpp::files::CONFIG_DIR_ENV, dir, 1) == 0 : unsetenv(xpp::files::CONFIG_DIR_ENV) == 0;
#endif
}

xpp::Session *load(const char *path)
{
    char arg0[] = "test_options";
    std::string file(path);
    char *argv[] = {arg0, file.data(), nullptr};
    xpp::Loaded l = xpp::load_model(2, argv, 1);
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
    if (r.color) return xpp::print_color(r.color(s));
    if (r.text) return r.text(s);
    return "";
}

/* the member lies in the Session (not in a process-wide setting such as
   the command line's batch_options) */
bool in_session(xpp::Session &s, const xpp::OptionRow &r)
{
    if (r.index) return true;
    const void *p = r.color ? static_cast<void *>(&r.color(s))
                  : r.real ? static_cast<void *>(&r.real(s))
                  : r.whole ? static_cast<void *>(&r.whole(s))
                  : r.boolean ? static_cast<void *>(&r.boolean(s))
                  : r.text ? static_cast<void *>(&r.text(s)) : nullptr;
    const char *b = reinterpret_cast<const char *>(&s);
    const char *q = static_cast<const char *>(p);
    return p && q >= b && q < b + sizeof(xpp::Session);
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
        {"SLIDERS", {"[]", ""}},
        {"S1", {"x", ""}}, {"S2", {"y", ""}}, {"S3", {"z", ""}},
        {"SLO1", {"-1", ""}}, {"SLO2", {"-1", ""}}, {"SLO3", {"-1", ""}},
        {"SHI1", {"2", ""}}, {"SHI2", {"2", ""}}, {"SHI3", {"2", ""}},
        {"DSMIN", {"0.001", "0.001"}}, {"DS", {"0.02", "0.02"}}, {"DSMAX", {"0.5", "0.5"}},
        {"METHOD", {"e", "1"}},            /* the Euler method */
        {"POINCARE_MAP", {"m", "2"}},          /* max */
        {"POINCARE_VARIABLE", {"y", "2"}},
        {"XP", {"y", "2"}},
        {"YP", {"z", "3"}},
        {"ZP", {"x", "1"}},
        {"AXES", {"3", "5"}},            /* 3D */
        {"LT", {"-1", "-1"}},
        {"PLOTFMT", {"svg", ""}},
        {"POINCARE_SIGN", {"-1", "0"}},         /* negative */
        {"LINE.DRAW", {"line", ""}}, {"LINE.DASH", {"solid", ""}}, {"LINE.WIDTH", {"1", ""}}, {"LINE.MARKER", {"circle", ""}},
        {"LINE.MARKER_SIZE", {"3", ""}},
        {"XNC", {"3", "#ffa500"}}, {"YNC", {"4", "#ffcd00"}}, {"SMC", {"1", "#ff0000"}}, {"UMC", {"0", "foreground"}},
        {"SEC", {"22", "#ffa500"}}, {"UEC", {"0", "foreground"}}, {"SPC", {"26", "#00e100"}}, {"UPC", {"28", "#0000ff"}},
        {"NULLCLINES.FILE", {"nc.dat", "nc.dat"}},
        {"RANGERESET", {"n", "false"}},
        {"RANGEOLDIC", {"n", "false"}},
        {"AUTOVAR", {"y", "1"}},         /* AUTO counts from x */
        {"HISTCOL", {"y", "2"}},
        {"HISTCOL2", {"z", "3"}},
        {"SPECCOL", {"y", "2"}},
        {"SPECCOL2", {"z", "3"}},
        {"QUIET", {"1", ""}},
        {"BELL", {"1", ""}},
        {"TUTORIAL", {"1", ""}},
        {"GRADS", {"1", ""}},
        {"FOLD", {"x", ""}},
        {"AUTOEVAL", {"1", ""}},
        {"JACOBIAN", {"exact", "1"}},
    };
    if (r.first_digit) return {r.name.ends_with('P') ? "y" : "1", ""}; /* xp2=y, xlo2=1 */
    if (const auto it = special.find(r.name); it != special.end()) return it->second;
    if (r.real) {
        const double v = 0.25 + static_cast<double>(k) / 64;
        return {xpp::number(v), xpp::number(v)};
    }
    if (r.whole) return {"2", "2"};
    if (r.boolean) return {"yes", "true"};
    if (r.text) return {"abc", "abc"};
    return {"1", ""};
}

/* what the option model leaves out: a log file, a button */
bool left_out(const xpp::OptionRow &r)
{
    return r.name.empty() || r.name == "LOGFILE" || r.name == "BUT" || r.kind == xpp::OptionKind::sliders || r.expand;
}

/* whether a .setx holds the option: the table's rows marked saved, and the
   torus' period (its torus group) */
bool in_set_file(const xpp::OptionRow &r) { return r.saved || r.name == "TORUS_PERIOD"; }

std::string_view id(const xpp::OptionRow &r) { return r.name.empty() ? r.key : r.name; }

} // namespace

int main(void)
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
        CHECK(xpp::check_command_line(2, argv).has_value());
        xpp::Session &session = xpp::client_session();
        xpp::batch_options.enabled = 0;
        CHECK(xpp::do_comline(session, 2, argv) == -1);
        CHECK(xpp::batch_options.enabled == 1);
        xpp::batch_options.enabled = 0;
        for (const char *name : {"silent", "setfile", "parfile", "icfile", "logfile", "include", "anifile", "outfile", "newseed", "version", "internset", "uset", "rset", "qsets", "qpars", "qics", "quiet", "mkplot", "plotfmt", "noout", "dfdraw", "ncdraw", "readset", "with", "equil", "verbose", "debug"}) {
            std::string word = "-" + std::string(name);
            argv[1] = word.data();
            const xpp::Result<> r = xpp::check_command_line(2, argv);
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
            const xpp::Result<> r = xpp::check_command_line(2, argv);
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
                const xpp::Result<> r = xpp::check_command_line(static_cast<int>(v.size()) - 1, v.data());
                return r ? std::string() : r.error().what;
            };
            CHECK(fault({"--equil", "x"}) == "--equil needs a whole number, not \"x\"");
            CHECK(fault({"--internset", "2"}) == "--internset must be 0 or 1, not 2");
            CHECK(fault({"--internset", "yes"}) == "--internset needs a whole number, not \"yes\"");
            CHECK(fault({"--parfile", "a", "--parfile", "b"}) == "--parfile given twice");
            CHECK(fault({"--setfile", "a", "--setfile", "b"}) == "--setfile given twice");
            CHECK(fault({"--readset", "a", "--with", "b"}) == "--with given with --readset: one of --readset and --with");
            CHECK(fault({"--uset", "a", "--uset", "b", "--include", "c", "--include", "d"}).empty());
            CHECK(fault({"--equil", "1", "--internset", "0", "model.ode"}).empty());
            CHECK(fault({"--equil"}) == "--equil needs an argument");
        }
        for (const char *word : {"--silently", "--nosuch", "--runnow", "--setfile"}) {
            std::string arg(word);
            argv[1] = arg.data();
            CHECK(!xpp::check_command_line(2, argv));
        }
    }

    for (const char *word : {"silent", "xorfix"}) {
        char exe[] = "test_options";
        std::string arg = "-" + std::string(word);
        char *argv[] = {exe, arg.data(), nullptr};
        const xpp::Loaded loaded = xpp::load_model(2, argv, 1);
        CHECK(!loaded);
        if (!loaded) CHECK(loaded.error().what.find(word) != std::string::npos);
    }

    /* Full registry names survive the .odex grammar and option reader. */
    for (const auto &info : xpp::solvers()) {
        if (info.traits.integral_history) continue; /* needs an integral model */
        CHECK(write_file("build/test_method_names.odex", xpp::format("x'=-x\ny'=x\noptions {{ integration: {{ method: {}, total_time: 1 }} }}\n", info.key)));
        xpp::Session *named = load("build/test_method_names.odex");
        CHECK(named && named->numerics.method == info.id && named->numerics.total_time == 1);
    }

    /* the limits and the nullclines' options: our own words in a .odex, XPPAUT's bound and ncdraw
       (each several of ours) in a .ode */
    CHECK(write_file("build/test_limits.odex", "x' = -x\noptions { integration: { state_limit: 7, stop_at_limit: false }, equilibria: { search_limit: 9 }, "
                                               "plot: { nullclines.draw: true, nullclines.file: \"nc.dat\" } }\n"));
    {
        const xpp::Session *l = load("build/test_limits.odex");
        CHECK(l && l->numerics.state_limit == 7 && !l->numerics.stop_at_limit && l->numerics.equilibria.search_limit == 9);
        CHECK(l && l->nullclines.draw && l->nullclines.file == "nc.dat");
    }
    for (const char *ncdraw : {"0", "1", "2"}) {
        CHECK(write_file("build/test_bound.ode", xpp::format("x'=-x\n@ bound=7 ncdraw={}\ndone\n", ncdraw)));
        const xpp::Session *l = load("build/test_bound.ode");
        CHECK(l && l->numerics.state_limit == 7 && l->numerics.stop_at_limit && l->numerics.equilibria.search_limit == 7);
        CHECK(l && l->nullclines.draw == (std::string_view(ncdraw) == "1") && l->nullclines.file == (std::string_view(ncdraw) == "2" ? "nullclines.dat" : ""));
    }
    /* XPPAUT's lt is the line style every new curve starts with: its line types; our own style is any */
    for (const int lt : {1, 0, -3}) {
        CHECK(write_file("build/test_lt.ode", xpp::format("x'=-x\n@ lt={}\ndone\n", lt)));
        const xpp::Session *l = load("build/test_lt.ode");
        CHECK(l && l->plot_settings.start_line_style == *xpp::line_style_of_xppaut(lt));
    }
    CHECK(write_file("build/test_lt.odex", "x' = -x\noptions { plot: { line.draw: points, line.marker_size: 4 } }\n"));
    {
        const xpp::Session *l = load("build/test_lt.odex");
        CHECK(l && l->plot_settings.start_line_style == *xpp::line_style_of_xppaut(-4));
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
        CHECK(l && l->plot_settings.start_line_style == want);
    }
    for (const char *bad : {"line.dash: dotted dashed", "line.width: 0", "line.marker: 5", "line.marker_size: -1", "line.draw: 1"}) {
        CHECK(write_file("build/test_bad_choice.odex", xpp::format("x' = -x\noptions {{ plot: {{ {} }} }}\n", bad)));
        char arg0[] = "test_options";
        char file[] = "build/test_bad_choice.odex";
        char *argv[] = {arg0, file, nullptr};
        CHECK(!xpp::load_model(2, argv, 1).has_value());
    }

    /* a number or a yes for a choice is no word of ours */
    for (const char *bad : {"poincare: { poincare_sign: 1 }", "poincare: { poincare_stop: 1 }", "integration: { stop_at_limit: yes }", "integration: { method: 3 }"}) {
        CHECK(write_file("build/test_bad_choice.odex", xpp::format("x' = -x\noptions {{ {} }}\n", bad)));
        char arg0[] = "test_options";
        char file[] = "build/test_bad_choice.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = xpp::load_model(2, argv, 1);
        CHECK(!l.has_value());
        if (!l) CHECK(l.error().place.line == 2);
    }

    /* each name finds its row */
    for (const xpp::OptionRow &r : rows) {
        for (std::string_view n : {r.name, r.alias, r.xppaut_names[0], r.xppaut_names[1]}) {
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
            CHECK(&xpp::once_option(r.name) == &r);
            for (const xpp::OptionRow &o : rows)
                CHECK(&o == &r || !o.once || o.name != r.name);
        }
    }
    /* the longest name wins: histlo2 is not histlo, dtmin not dt */
    {
        int index = 0;
        CHECK(xpp::find_option("HISTLO2", index, true)->name == "HISTLO2");
        CHECK(xpp::find_option("DTMINIMUM", index, true)->name == "MIN_STEP");
        CHECK(xpp::find_option("METHOD", index, true)->name == "METHOD" && xpp::find_option("METH", index, true)->name == "METHOD");
        CHECK(xpp::find_option("XP9", index, true)->name == "XP");
        CHECK(xpp::find_option("XP3", index, true)->first_digit == '2' && index == 3);
        CHECK(xpp::find_option("NOSUCH", index, true) == nullptr);
        CHECK(xpp::find_option("NOUT", index, true)->name == "STORE_EVERY" && xpp::find_option("NJMP", index, true) == xpp::find_option("NOUT", index, true));
        CHECK(xpp::find_option("NOUT", index, false) == nullptr && xpp::odex_option_key("NJMP") == "store_every" && xpp::odex_option_key("dt") == "dt"
               && xpp::odex_option_key("DTMIN") == "min_step" && xpp::odex_option_key("meth") == "method" && xpp::odex_option_key("method") == "method");
        /* a prefix of a row's name is its key too (xplot, bounds), a name no group has is none */
        CHECK(xpp::odex_option_key("xplot") == "xp" && xpp::odex_option_key("XP3") == "xp3" && xpp::odex_option_key("bell").empty()
               && xpp::odex_option_key("nosuch").empty());
        CHECK(xpp::find_odex_option("xp3", index) && index == 3 && !xpp::find_odex_option("xplot", index) && !xpp::find_odex_option("total", index));
    }

    /* the defaults a model with no options gets */
    CHECK(write_file(plain_ode, std::string(model_text) + "done\n"));
    xpp::Session *s = load(plain_ode);
    CHECK(s != nullptr);
    /* a model's own set or action takes the new names only; the command line's settings take XPPAUT's too */
    if (s) {
        CHECK(xpp::option_problem(*s, "total", "5", false).has_value());
        CHECK(!xpp::option_problem(*s, "total", "5", true).has_value());
        CHECK(!xpp::option_problem(*s, "total_time", "5", false).has_value());
        CHECK(xpp::option_problem(*s, "meth", "e", false).has_value() && !xpp::option_problem(*s, "method", "euler", false).has_value());
        CHECK(!xpp::option_problem(*s, "method", "euler", false).has_value());
        /* an XPPAUT choice's letter, number or yes/no is a foreign file's, never our own */
        CHECK(!xpp::option_problem(*s, "meth", "e", true).has_value() && xpp::option_problem(*s, "method", "1", false).has_value());
        CHECK(xpp::option_problem(*s, "poincare_sign", "1", false).has_value() && !xpp::option_problem(*s, "poincare_sign", "positive", false).has_value());
        CHECK(!xpp::option_problem(*s, "poisgn", "1", true).has_value() && !xpp::option_problem(*s, "poistop", "1", true).has_value());
        CHECK(xpp::option_problem(*s, "poincare_stop", "1", false).has_value() && xpp::option_problem(*s, "rangereset", "yes", false).has_value());
        CHECK(!xpp::option_problem(*s, "rangereset", "true", false).has_value() && !xpp::option_problem(*s, "rangereset", "y", true).has_value());
    }
    if (!s) TEST_REPORT("options");
    std::vector<std::string> defaults;
    for (const xpp::OptionRow &r : rows) defaults.push_back(value_of(*s, r));
    for (const xpp::OptionRow &r : rows)
        if (r.once && !r.reset && r.name != "COLORMAP") CHECK(!s->options_set.has(r)); /* an .ode's conversion sets its colormap, jet */

    /* every option set from @ lines, the last row's first (xmin sets the
       2D window's xlo too; xlo's own line after it wins) */
    std::string text(model_text);
    for (std::size_t k = rows.size(); k-- > 0;) {
        const xpp::OptionRow &r = rows[k];
        if (left_out(r)) continue;
        std::string name(r.name);
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

    /* a .set file brings back what it holds */
    init_browser(*s);
    init_all_graph(*s);
    for (std::size_t k = 0; k < rows.size(); k++) {
        const xpp::OptionRow &r = rows[k];
        if (r.name.empty() && r.key.empty()) continue;
        const bool held = in_set_file(r);
        CHECK(!held || in_session(*s, r));
        if (!r.key.empty()) CHECK(held); /* every numerics setting the menu edits is in a .setx */
    }
    const xpp::Result<std::string> own = xpp::setx_text(*s, xpp::capture_settings(*s, xpp::SettingsUse::file));
    CHECK(own.has_value());
    if (!own) TEST_REPORT("options");
    CHECK(xpp::read_settings(*s, set_file, *own, xpp::SettingsUse::file).has_value());
    CHECK(write_file(set_file, *own));
    std::vector<std::string> written;
    for (const xpp::OptionRow &r : rows) {
        written.push_back(value_of(*s, r));
        if (!in_session(*s, r)) continue;
        if (r.real) r.real(*s) += 1;
        else if (r.whole && r.saved && r.kind == xpp::OptionKind::choice) r.whole(*s) = (r.whole(*s) + 1) % static_cast<int>(xpp::option_choices(r).size());
        else if (r.whole) r.whole(*s) += 1;
        else if (r.boolean) r.boolean(*s) = !r.boolean(*s);
        else if (r.index) r.index.write(*s, (r.index.read(*s) + 1) % static_cast<int>(xpp::option_choices(r).size()));
        else if (r.color) r.color(*s) = xpp::xppaut_color(5);
        else if (r.text) r.text(*s) = "xyz";
    }
    std::vector<std::string> changed;
    for (const xpp::OptionRow &r : rows) changed.push_back(value_of(*s, r));
    CHECK(xpp::import_settings(*s, set_file, false).has_value());
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
    for (const char *bad : {"dt=abc", "ync=12", "xp=nosuch", "meth=k", "store_every=2.5", "nout=2.5", "rangereset=maybe",
                            "axes=4", "quiet=2", "seed=-1", "lt=3", "histcol=nosuch", "xlo2=1e",
                            /* a number the setting's rule refuses (OptionRule) */
                            "store_every=0", "njmp=0", "dt=0", "nmesh=0", "bound=-1", "delay=-1", "tol=0"}) {
        CHECK(write_file(bad_ode, std::string(model_text) + "@ total=5\n@ " + bad + "\ndone\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.ode";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = xpp::load_model(2, argv, 1);
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
        if (!row.maximum) continue;
        CHECK(!xpp::option_problem(*s, row.name, std::to_string(row.maximum), false));
        const std::string value = std::to_string(row.maximum + 1);
        for (bool native : {false, true}) {
            const char *path = native ? bad_odex : bad_ode;
            const std::string text = native ? "x' = -x\noptions { " + std::string(row.group) + ": { " + xpp::odex_key(row, 0) + ": " + value + " } }\n"
                                            : "x'=-x\n@ " + std::string(row.name) + "=" + value + "\ndone\n";
            CHECK(write_file(path, text));
            char arg0[] = "test_options";
            std::string file(path);
            char *argv[] = {arg0, file.data(), nullptr};
            const xpp::Loaded refused = xpp::load_model(2, argv, 1);
            CHECK(!refused);
            if (!refused) {
                CHECK(refused.error().place.file == path && refused.error().place.line == 2);
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
        const auto refused = xpp::load_model(2, argv, 1);
        CHECK(!refused);
        if (!refused) {
            CHECK(refused.error().place.file == bad_ode && refused.error().place.line == 1);
            CHECK(refused.error().what.find("maximum 100000") != std::string::npos);
        }
    }
    CHECK(write_file(bad_odex, "par a = 1\nx' = -a*x\noptions { plot: { ync: 12 } }\n"));
    {
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = xpp::load_model(2, argv, 1);
        CHECK(!l.has_value());
        CHECK(&xpp::client_session() == s);
        if (!l) {
            CHECK(l.error().place.file == bad_odex);
            CHECK(l.error().place.line == 3);
            CHECK(l.error().what.find("double quotes") != std::string::npos);
            if (l.error().place.line != 3) printf("  odex: %s\n", l.error().text().c_str());
        }
    }
    /* Continuation rules are shared with the form and session reader; a bad
       complete model leaves the previous client session untouched. */
    for (const std::string setting : {"dsmin: -0.001", "dsmax: 0", "ds: 0", "epsl: 0", "epsu: -1", "epss: 0",
                                     "ncol: 1", "npr: 0", "dsmin: 0.2, dsmax: 0.1", "ds: 0.8, dsmax: 0.5"}) {
        CHECK(write_file(bad_odex, "x' = -x\noptions { continuation: { " + setting + " } }\n"));
        char arg0[] = "test_options", file[] = "build/test_options_bad.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded loaded = xpp::load_model(2, argv, 1);
        CHECK(!loaded);
        CHECK(&xpp::client_session() == s);
        if (!loaded) CHECK(loaded.error().place.file == bad_odex && loaded.error().place.line == 2);
    }
    for (const std::string color : {"foreground", "red"}) {
        CHECK(write_file(bad_odex, "x' = -x\noptions { plot: { xnc: " + color + " } }\n"));
        char arg0[] = "test_options", file[] = "build/test_options_bad.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded loaded = xpp::load_model(2, argv, 1);
        CHECK(!loaded);
        if (!loaded) CHECK(loaded.error().place.line == 2);
    }
    /* XPPAUT's names for the renamed options are a .ode's: a .odex refuses them
       at their line */
    for (const char *old_name : {"nout", "njmp", "total", "t0", "trans", "nmesh", "newt_iter", "newt_tol", "jac_eps", "bound", "ncdraw", "meth", "tol", "dtmin", "dtmax", "atol", "maxstor", "tor_per", "poimap", "poivar", "poisgn", "poistop", "poipln"}) {
        CHECK(write_file(bad_odex, std::string("par a = 1\nx' = -a*x\noptions { integration: { ") + old_name + ": 2 } }\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = xpp::load_model(2, argv, 1);
        CHECK(!l.has_value());
        if (l) continue;
        CHECK(l.error().place.file == bad_odex);
        CHECK(l.error().place.line == 3);
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
        const xpp::Loaded l = xpp::load_model(2, argv, 1);
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
       line's own (the model's file and line), and the --include flag's */
    {
        const xpp::Session *before = &xpp::client_session();
        CHECK(write_file(bad_ode, std::string(model_text) + "#include nosuch.inc\ndone\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.ode";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = xpp::load_model(2, argv, 1);
        CHECK(!l.has_value());
        CHECK(&xpp::client_session() == before);
        if (!l) {
            CHECK(l.error().place.file == bad_ode);
            CHECK(l.error().place.line == 5);
            CHECK(l.error().what.find("nosuch.inc") != std::string::npos);
            if (l.error().place.line != 5) printf("  include: %s\n", l.error().text().c_str());
        }
        CHECK(write_file(bad_ode, std::string(model_text) + "done\n"));
        char flag[] = "--include";
        char missing[] = "nosuch.inc";
        char *argv2[] = {arg0, file, flag, missing, nullptr};
        const xpp::Loaded f = xpp::load_model(4, argv2, 1);
        CHECK(!f.has_value());
        CHECK(&xpp::client_session() == before);
        if (!f) {
            CHECK(f.error().place.file == "nosuch.inc");
            CHECK(f.error().what.find("--include") != std::string::npos);
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
            const std::string text = odex ? xpp::format("x' = -x\noptions {{ output: {{ output: \"{}\" }} }}\n", c.value)
                                          : std::string(model_text) + "@ total=5\n@ output=" + c.value + "\ndone\n";
            CHECK(write_file(file_name, text));
            char arg0[] = "test_options";
            char arg1[64];
            std::snprintf(arg1, sizeof arg1, "%s", file_name);
            char *argv[] = {arg0, arg1, nullptr};
            const xpp::Loaded l = xpp::load_model(2, argv, 1);
            if (*c.written) {
                CHECK(l.has_value());
                CHECK((*l)->batch_out_file == c.written);
                continue;
            }
            CHECK(!l.has_value());
            if (l) continue;
            CHECK(l.error().place.file == file_name);
            CHECK(l.error().place.line == (odex ? 2 : 6));
            CHECK(l.error().what.find(c.value) != std::string::npos);
            if (l.error().place.line != (odex ? 2 : 6)) printf("  output %s: %s\n", c.value, l.error().text().c_str());
        }
    }

    /* a path outside the folder is a question (W286), asked once, in a
       config folder of its own: no answer refuses at the line; -y allows
       it once and says so; "o" allows this load; "a" is kept for this model
       file and this path, so the next load does not ask, and another path asks */
    {
        const auto load_output = [&](const char *path) {
            CHECK(write_file(bad_odex, std::string("x' = -x\noptions { output: { output: \"") + path + "\" } }\n"));
            char arg0[] = "test_options";
            char arg1[64];
            std::snprintf(arg1, sizeof arg1, "%s", bad_odex);
            char *argv[] = {arg0, arg1, nullptr};
            return xpp::load_model(2, argv, 1);
        };
        const char outside[] = "../simulations/out.dat", other[] = "../simulations/other.dat";
        CHECK(answer_with(""));
        xpp::Loaded l = load_output(outside);
        CHECK(!l.has_value());
        if (!l) {
            CHECK(l.error().place.file == bad_odex && l.error().place.line == 2);
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
    TEST_REPORT("options");
}
