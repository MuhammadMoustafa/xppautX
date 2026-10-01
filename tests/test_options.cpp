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
#include "model_options.h"
#include "lunch-new.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "browse.h"
#include "graphics.h"

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

const char plain_ode[] = "build/test_options_plain.ode";
const char options_ode[] = "build/test_options_set.ode";
const char set_file[] = "build/test_options.set";

/* a model with three variables and no options */
const char model_text[] = "x'=-x\ny'=x\nz'=y\npar a=1,b=2\n";

bool write_file(const char *path, const std::string &text)
{
    xpp::Writer w(path);
    return w && w.write(text) && w.commit();
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
    if (r.text) return r.text(s);
    return "";
}

/* the member lies in the Session (not a process-wide setting such as
   program.tutorial, which a load keeps) */
bool in_session(xpp::Session &s, const xpp::OptionRow &r)
{
    const void *p = r.real ? static_cast<void *>(&r.real(s))
                  : r.whole ? static_cast<void *>(&r.whole(s))
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
        {"METH", {"e", "1"}},            /* the Euler method */
        {"POIMAP", {"m", "2"}},          /* max */
        {"POIVAR", {"y", "2"}},
        {"XP", {"y", "2"}},
        {"YP", {"z", "3"}},
        {"ZP", {"x", "1"}},
        {"AXES", {"3", "5"}},            /* 3D */
        {"LT", {"-1", "-1"}},
        {"RANGERESET", {"n", "0"}},
        {"RANGEOLDIC", {"n", "0"}},
        {"AUTOVAR", {"y", "1"}},         /* AUTO counts from x */
        {"HISTCOL", {"y", "2"}},
        {"HISTCOL2", {"z", "3"}},
        {"SPECCOL", {"y", "2"}},
        {"SPECCOL2", {"z", "3"}},
        {"TUTORIAL", {"1", "1"}},
        {"QUIET", {"1", ""}},
        {"BELL", {"1", ""}},
        {"GRADS", {"1", ""}},
        {"FOLD", {"x", ""}},
        {"AUTOEVAL", {"1", ""}},
    };
    if (r.first_digit) return {r.name.ends_with('P') ? "y" : "1", ""}; /* xp2=y, xlo2=1 */
    if (const auto it = special.find(r.name); it != special.end()) return it->second;
    if (r.real) {
        const double v = 0.25 + static_cast<double>(k) / 64;
        return {xpp::number(v), xpp::number(v)};
    }
    if (r.whole) return {"2", "2"};
    if (r.text) return {"abc", "abc"};
    return {"1", ""};
}

/* what the option model leaves out: a log file, a button */
bool left_out(const xpp::OptionRow &r)
{
    return r.name.empty() || r.name == "LOGFILE" || r.name == "BUT";
}

/* the options a .set file holds (lunch-new.cpp io_numerics, dump_torus,
   dump_range) */
const std::set<std::string_view> in_set_file = {
    "TOTAL", "T0", "TRANS", "DT", "NMESH", "NEWT_ITER", "NEWT_TOL", "JAC_EPS",
    "NOUT", "BOUND", "METH", "TOL", "DTMIN", "DTMAX", "ATOL", "DELAY",
    "bvp_maxit", "bvp_tol", "bvp_eps", "POIMAP", "POIVAR", "POISGN",
    "POISTOP", "POIPLN", "TOR_PER", "RANGEOVER", "RANGESTEP", "RANGELOW",
    "RANGEHIGH", "RANGERESET", "RANGEOLDIC",
};
/* the numerics a .set file does not hold */
const std::set<std::string_view> not_in_set_file = {"eul_tol", "eul_iter"};

std::string_view id(const xpp::OptionRow &r) { return r.name.empty() ? r.key : r.name; }

} // namespace

int main(void)
{
    const std::span<const xpp::OptionRow> rows = xpp::option_rows();

    /* each name finds its row */
    for (const xpp::OptionRow &r : rows) {
        for (std::string_view n : {r.name, r.alias}) {
            if (n.empty()) continue;
            int index = -1;
            std::string name(n);
            if (r.first_digit) name += r.last_digit;
            const xpp::OptionRow *found = xpp::find_option(name, index);
            CHECK(found == &r);
            if (found != &r) printf("  %s finds another row\n", name.c_str());
            if (r.first_digit) CHECK(index == r.last_digit - '0');
        }
        if (!r.key.empty()) CHECK(xpp::numerics_option(r.key) == &r);
        /* a numbered row is its name and a digit; a flag is one row's */
        if (r.flag != xpp::Option::none)
            for (const xpp::OptionRow &o : rows)
                CHECK(&o == &r || o.flag != r.flag);
    }
    /* the longest name wins: histlo2 is not histlo, dtmin not dt */
    {
        int index = 0;
        CHECK(xpp::find_option("HISTLO2", index)->name == "HISTLO2");
        CHECK(xpp::find_option("DTMINIMUM", index)->name == "DTMIN");
        CHECK(xpp::find_option("METHOD", index)->name == "METH");
        CHECK(xpp::find_option("XP9", index)->name == "XP");
        CHECK(xpp::find_option("XP3", index)->first_digit == '2' && index == 3);
        CHECK(xpp::find_option("NOSUCH", index) == nullptr);
    }

    /* the defaults a model with no options gets */
    CHECK(write_file(plain_ode, std::string(model_text) + "done\n"));
    xpp::Session *s = load(plain_ode);
    CHECK(s != nullptr);
    if (!s) TEST_REPORT("options");
    std::vector<std::string> defaults;
    for (const xpp::OptionRow &r : rows) defaults.push_back(value_of(*s, r));
    for (const xpp::OptionRow &r : rows)
        if (r.flag != xpp::Option::none && !r.reset) CHECK(!s->options_set.has(r.flag));

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
        if (r.flag != xpp::Option::none) CHECK(s->options_set.has(r.flag));
    }

    /* a .set file brings back what it holds */
    init_browser(*s);
    init_all_graph(*s);
    for (std::size_t k = 0; k < rows.size(); k++) {
        const xpp::OptionRow &r = rows[k];
        if (r.name.empty() && r.key.empty()) continue;
        const bool held = in_set_file.contains(id(r));
        CHECK(!held || in_session(*s, r));
        if (!r.key.empty() && r.name.empty()) CHECK(held || not_in_set_file.contains(r.key));
    }
    {
        FILE *fp = fopen(set_file, "w");
        CHECK(fp != nullptr);
        if (!fp) TEST_REPORT("options");
        xpp::write_lunch(*s, fp);
        fclose(fp);
    }
    std::vector<std::string> written;
    for (const xpp::OptionRow &r : rows) {
        written.push_back(value_of(*s, r));
        if (!in_session(*s, r)) continue;
        if (r.real) r.real(*s) += 1;
        else if (r.whole) r.whole(*s) += 1;
        else if (r.text) r.text(*s) = "xyz";
    }
    std::vector<std::string> changed;
    for (const xpp::OptionRow &r : rows) changed.push_back(value_of(*s, r));
    {
        FILE *fp = fopen(set_file, "r");
        CHECK(fp != nullptr);
        if (!fp) TEST_REPORT("options");
        CHECK(xpp::read_lunch(*s, fp, false).has_value());
        fclose(fp);
    }
    for (std::size_t k = 0; k < rows.size(); k++) {
        const xpp::OptionRow &r = rows[k];
        if (!in_session(*s, r)) continue;
        const std::string want = in_set_file.contains(id(r)) ? written[k] : changed[k];
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
    for (const char *bad : {"dt=abc", "ync=12", "xp=nosuch", "meth=k", "nout=2.5", "rangereset=maybe",
                            "axes=4", "quiet=2", "seed=-1", "lt=3", "histcol=nosuch", "xlo2=1e"}) {
        CHECK(write_file(bad_ode, std::string(model_text) + "@ total=5\n@ " + bad + "\ndone\n"));
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.ode";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = xpp::load_model(2, argv, 1);
        CHECK(!l.has_value());
        CHECK(&xpp::client_session() == s);
        if (l) continue;
        const std::string value(std::string_view(bad).substr(std::string_view(bad).find('=') + 1));
        CHECK(l.error().file == bad_ode);
        CHECK(l.error().line == 6);
        CHECK(l.error().cause.find(value) != std::string::npos);
        if (l.error().line != 6 || l.error().cause.find(value) == std::string::npos)
            printf("  %s: %s\n", bad, l.error().text().c_str());
    }
    CHECK(write_file(bad_odex, "par a = 1\nx' = -a*x\n@ ync=12\n"));
    {
        char arg0[] = "test_options";
        char file[] = "build/test_options_bad.odex";
        char *argv[] = {arg0, file, nullptr};
        const xpp::Loaded l = xpp::load_model(2, argv, 1);
        CHECK(!l.has_value());
        CHECK(&xpp::client_session() == s);
        if (!l) {
            CHECK(l.error().file == bad_odex);
            CHECK(l.error().line == 3);
            CHECK(l.error().cause.find("ync=12") != std::string::npos);
            if (l.error().line != 3) printf("  odex: %s\n", l.error().text().c_str());
        }
    }
    CHECK(xpp::client_session().model().this_file == plain_ode);

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
        CHECK(l.error().file == bad_ode);
        CHECK(l.error().line == 5);
        CHECK(l.error().cause.find("foo.opt") != std::string::npos);
        CHECK(l.error().cause.find("@ lines") != std::string::npos);
        if (l.error().line != 5) printf("  %s: %s\n", word, l.error().text().c_str());
    }

    /* the settings go in @ lines of an include file (an .odex's include),
       which apply */
    {
        const char inc_file[] = "build/test_options_inc.incx";
        CHECK(write_file(inc_file, "@ total=7\n@ dt=0.25\n"));
        CHECK(write_file(bad_odex, "par a = 1\nx' = -a*x\ninclude \"test_options_inc.incx\"\n"));
        xpp::Session *t = load(bad_odex);
        CHECK(t != nullptr);
        if (t) {
            int index = 0;
            CHECK(value_of(*t, *xpp::find_option("TOTAL", index)) == xpp::number(7.0));
            CHECK(value_of(*t, *xpp::find_option("DT", index)) == xpp::number(0.25));
        }
        remove(inc_file);
    }

    remove(bad_ode);
    remove(bad_odex);
    remove(plain_ode);
    remove(options_ode);
    remove(set_file);
    TEST_REPORT("options");
}
