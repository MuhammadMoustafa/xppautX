/* The settings file (.setx, setx_file.h, the session's model.setx) and XPPAUT's set file, the import
   (lunch-new.h). XPPAUT's own examples/ode/lecar.ode.set is imported (File >
   Import XPPAUT session state, --setfile): read and applied whole, nothing written
   beside it, and the session then holds every value the set file named.
   The settings written again, read, applied and written once more give the
   same file. A file with a value that is wrong, a key that is not one of the
   model's or a group written twice is refused at its line and column and
   changes nothing (W125); so is an XPPAUT set file cut short, with a line
   that is not a number or with a name that is not the model's.

   The parameter file (.par) and the initial-conditions file (.ic) are read
   the same way. The model is loaded the way xppautX --silent loads it
   (xpp::load_model), without integrating. make test runs this from the top
   of the tree. */
#include "load_args.h"
#include "xpptest.h"
#include "session.h"
#include "solver.h"
#include "lunch-new.h"
#include "session_settings.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include "expr.h"
#include "browse.h"
#include "graphics.h"
#include "load_eqn.h"
#include "xpp_files.h"
#include <stdio.h>
#include <string>
#include <tuple>

namespace {

/* path's bytes, "" when it cannot be read */
std::string bytes_of(const char *path)
{
    std::string b;
    return xpp::read_bytes(path, b) ? b : std::string();
}

void put(const char *path, const std::string &text)
{
    xpp::Writer w = xpp::Writer::binary(path);
    CHECK(w && w.write(text) && w.commit());
}

/* the settings of the session now, as a .setx's text */
std::string snapshot(const xpp::Session &s)
{
    const xpp::Result<std::string> t = xpp::settings_text(s, xpp::capture_settings(s, xpp::SettingsUse::session));
    CHECK(t.has_value());
    return t ? *t : std::string();
}

/* text with its line n (from 1) replaced by line */
std::string with_line(const std::string &text, int n, const std::string &line)
{
    std::size_t at = 0;
    for (int k = 1; k < n; k++) at = text.find('\n', at) + 1;
    return text.substr(0, at) + line + text.substr(text.find('\n', at));
}

/* the number (from 1) of the first line of text, after line `after`, that contains what */
int line_with(const std::string &text, const std::string &what, int after = 0)
{
    int n = 0;
    for (const std::string_view line : xpp::split_lines(text)) {
        n++;
        if (n > after && line.find(what) != std::string_view::npos) return n;
    }
    return 0;
}

/* the settings file at path read and applied (a session file's model.setx is read the same way) */
xpp::Result<> apply_setx(xpp::Session &s, const char *path)
{
    const std::string text = bytes_of(path);
    const xpp::Result<xpp::Settings> f = xpp::read_settings(s, path, text);
    if (!f) return std::unexpected(f.error());
    xpp::apply_settings(s, *f, false);
    return {};
}

/* the error of path (text put there) as a settings file, "" when it loads */
std::string setx_error(xpp::Session &s, const char *path, const std::string &text)
{
    put(path, text);
    const xpp::Result<> r = apply_setx(s, path);
    return r ? std::string() : r.error().text(); /* "path:line:col: what" */
}

/* the same for XPPAUT's set file imported */
std::string set_error(xpp::Session &s, const char *path, const std::string &text)
{
    put(path, text);
    const auto r = xpp::import_xppaut_state(s, path, false);
    return r ? std::string() : r.error().text();
}

} // namespace

int main(void)
{
    xpp::XppUi test_ui{};
    test_ui.save_replace = [](bool) -> int { return xpp::SAVE_REPLACE; };
    xpp::set_ui(&test_ui); /* these writes target this test's scratch files */
    char arg0[] = "test_lunch", arg1[] = "examples/ode/lecar.odex";
    char *argv[] = {arg0, arg1, NULL};
    const char *xs = "build/test_lunch_a.set", *xsx = "build/test_lunch_a.setx", *own = "build/test_lunch_own.setx";
    double x;

    CHECK(load_args(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    init_browser(s);
    init_all_graph(s);

    /* XPPAUT's set file imported whole, nothing written beside it (the 2013 file counts the model's 4 equations and
       auxiliaries, the model now has 6) */
    std::string set = bytes_of("examples/ode/lecar.ode.set");
    set.replace(set.find("4   Number of equations"), 1, "6");
    put(xs, set);
    remove(xsx);
    const auto imported = xpp::import_xppaut_state(s, xs, false);
    CHECK(imported.has_value());
    if (!imported) printf("  %s\n", imported.error().text().c_str());
    CHECK(!xpp::files::exists(xsx));
    CHECK(imported && !imported->empty()); /* it names what it changed */
    CHECK(s.numerics.total_time == 500 && s.numerics.delta_t == 0.05 && s.numerics.store_from == 200);
    CHECK(s.numerics.method == xpp::method::RK4);
    CHECK(s.data_store.current_time == 8.549999999999986);
    xpp::get_val(s, "iapp", &x);
    CHECK(x == 0.09);
    xpp::get_val(s, "gca", &x);
    CHECK(x == 1);
    CHECK(s.last_ic[0] == 0.008015491512486121 && s.last_ic[1] == 0.05732158561413084);
    CHECK(s.numerics.poincare_map == xpp::PoincareMap::period); /* Period */
    CHECK(s.numerics.jacobian == 0);

    /* W347: XPPAUT's negative Total remains a finite absolute duration. */
    put(xs,with_line(set,line_with(set,"  total"),"-500  total"));
    remove(xsx);
    CHECK(xpp::import_xppaut_state(s,xs,false).has_value());
    CHECK(s.numerics.total_time==500);

    /* the .setx written again, read, applied and written once more: the same */
    const std::string first = snapshot(s);
    CHECK(first.size() > 500);
    put(own, first);
    CHECK(apply_setx(s, own).has_value());
    CHECK(snapshot(s) == first);
    /* every part the file names is there, by name */
    CHECK(first.find("  total_time: 500,\n") != std::string::npos && first.find("  jacobian: difference,\n") != std::string::npos);
    CHECK(first.find("  poincare_map: period,\n") != std::string::npos && first.find("  iapp: 0.09,\n") != std::string::npos);

    /* Our settings refuse a negative Total without applying any values. */
    for(const char *total : {"-5","-0.5"}){
        const int line=line_with(first,"  total_time:");
        const std::string bad=with_line(first,line,"  total_time: "+std::string(total)+",");
        CHECK(setx_error(s,own,bad).starts_with(xpp::format("{}:{}:",own,line)));
        CHECK(snapshot(s)==first);
    }

    /* Saved dimensions and numerics are refused with their member's place. */
    const auto session_text = xpp::settings_text(s, xpp::capture_settings(s, xpp::SettingsUse::session));
    CHECK(session_text);
    for (const char *group : {"array_plot", "transpose"}) {
        for (const char *key : {"columns", "rows"}) {
            if (!session_text) break;
            const int in_group = line_with(*session_text, xpp::format("{}: {{", group));
            const int line = line_with(*session_text, xpp::format("  {}:", key), in_group);
            CHECK(in_group > 0 && line > in_group);
            const auto r = xpp::read_settings(s, "sent.setx", with_line(*session_text, line, xpp::format("  {}: {},", key, xpp::settings_dimension_limit + 1)));
            CHECK(!r);
            if (!r) {
                CHECK(r.error().place.file == "sent.setx" && r.error().place.line == line);
                CHECK(r.error().what.find("4097") != std::string::npos);
            }
        }
    }
    if (session_text) {
        const int line = line_with(*session_text, "nullcline_mesh:");
        const auto mesh = xpp::read_settings(s, "session.snapx/model.setx",
            with_line(*session_text, line, "  nullcline_mesh: 4097,"));
        CHECK(!mesh);
        if (!mesh) CHECK(mesh.error().place.file == "session.snapx/model.setx" && mesh.error().place.line == line);
    }
    const int mesh_line = line_with(set, "nullcline mesh");
    const std::string bad_mesh = with_line(set, mesh_line, "4097 nullcline mesh");
    put(xs, bad_mesh);
    const auto imported_mesh = xpp::import_xppaut_state(s, xs, false);
    CHECK(!imported_mesh);
    if (!imported_mesh) CHECK(imported_mesh.error().place.file == xs && imported_mesh.error().place.line == mesh_line);
    for (const char *label : {"n columns", "n rows", "NCols", "NRows"}) {
        const int line = line_with(set, label);
        put(xs, with_line(set, line, xpp::format("{} {}", xpp::settings_dimension_limit + 1, label)));
        const auto r = xpp::import_xppaut_state(s, xs, false);
        CHECK(!r);
        if (!r) CHECK(r.error().place.file == xs && r.error().place.line == line);
    }
    put(xs, set);

    /* all or nothing: the session changed, then a file with a bad value last
       is refused at that line, the session as it was, value for value */
    xpp::set_val(s, "iapp", 0.25);
    s.numerics.total_time = 11;
    const std::string before = snapshot(s);
    const int last_line = line_with(first, "high:");
    CHECK(last_line > 50);
    const std::string last_bad = with_line(first, last_line, "  high: 1e999,");
    const std::string last_error = setx_error(s, own, last_bad);
    CHECK(last_error.starts_with(xpp::format("{}:{}:", own, last_line)) && last_error.find("1e999") != std::string::npos);
    CHECK(snapshot(s) == before);
    /* a value a numerics rule refuses, a key the model has not, a group
       twice, an unknown key: each at its line, nothing applied */
    for (const auto &[line, value, why] : {std::tuple{"  dt: ", "  dt: 0,", "Dt must be a number other than 0"},
                                           std::tuple{"  store_every: ", "  store_every: 0,", "Store every N steps must be a whole number of at least 1"},
                                           std::tuple{"  state_limit: ", "  state_limit: -1,", "State limit must be a number above 0"},
                                           std::tuple{"  method: ", "  method: nosuch,", "nosuch"},
                                           std::tuple{"  iapp: ", "  nosuchpar: 1,", "`nosuchpar` is not one of the model's parameters"},
                                           std::tuple{"  iapp: ", "  Iapp: 1,", "`Iapp` is not one of the model's parameters"},
                                            std::tuple{"  iapp: ", "  iapp: abc,", "`iapp`: `abc` is not a number"}}) {
        const int n = line_with(first, line);
        CHECK(n > 0);
        const std::string e = setx_error(s, own, with_line(first, n, value));
        CHECK(e.starts_with(xpp::format("{}:{}:", own, n)) && e.find(why) != std::string::npos);
        if (e.find(why) == std::string::npos) printf("  %s\n", e.c_str());
        CHECK(snapshot(s) == before);
    }
    CHECK(setx_error(s, own, first + "numerics: {},\n").find("`numerics` is given twice") != std::string::npos);
    CHECK(setx_error(s, own, first + "nosuch: {},\n").find("`nosuch` is not a setting") != std::string::npos);
    CHECK(!setx_error(s, own, "total_time: 5,\n").empty()); /* outside its group */
    CHECK(setx_error(s, own, "numerics: { total_time: 5 \n").find("expected") != std::string::npos);
    CHECK(snapshot(s) == before);

    /* XPPAUT's set file: cut short, a value that is no number, a name that
       is not the model's: refused at its line, nothing applied */
    const std::string cut = set.substr(0, set.find("# Delays"));
    const std::string short_error = set_error(s, xs, cut);
    CHECK(short_error.starts_with(std::string(xs) + ":") && short_error.find("the file ends here") != std::string::npos);
    const std::string bad_total = set_error(s, xs, with_line(set, 8, "x  total"));
    CHECK(bad_total.starts_with(std::string(xs) + ":8:") && bad_total.find("is not a number") != std::string::npos);
    const std::string wrong_name = set_error(s, xs, with_line(set, 5, "40  another_model_name"));
    CHECK(wrong_name.starts_with(std::string(xs) + ":5:") && wrong_name.find("another_model_name") != std::string::npos);
    const std::string no_trailer = set_error(s, xs, set.substr(0, set.find("RHS etc")));
    CHECK(no_trailer.find("the file ends here") != std::string::npos);
    CHECK(snapshot(s) == before);

    /* a parameter file and an initial-conditions file: read whole as well */
    const xpp::Model &m = s.model();
    const char *par = "build/test_lunch.par", *ic = "build/test_lunch.ic";
    const std::string kept = snapshot(s);
    std::string par_text = xpp::format("{}   Number params\n", m.nupar);
    for (int i = 0; i < m.nupar; i++) par_text += xpp::format("{}  {}\n", i == m.nupar - 1 ? "x" : "1", m.upar_names[i]);
    put(par, par_text);
    const xpp::Result<std::vector<double>> pr = xpp::read_parameter_file(m, par);
    CHECK(!pr && pr.error().text() == xpp::format("{}:{}: \"x  {}\" is not a number ({})", par, m.nupar + 1,
                                                  m.upar_names[m.nupar - 1], m.upar_names[m.nupar - 1]));
    CHECK(!xpp::load_xppaut_values(s, xpp::ValueFile::parameters, par));
    CHECK(snapshot(s) == kept);
    /* its trailer, as XPPAUT's file ends: the file, then the time it was written, then the end */
    std::string values = xpp::format("{}   Number params\n", m.nupar);
    for (int i = 0; i < m.nupar; i++) values += xpp::format("1  {}\n", m.upar_names[i]);
    put(par, values + "\n\nFile:lecar.odex\nWed Jun  3 21:49:08 1993\n");
    CHECK(xpp::read_parameter_file(m, par).has_value());
    put(par, values + "\n\nFile:lecar.odex\nyesterday\n");
    const xpp::Result<std::vector<double>> no_time = xpp::read_parameter_file(m, par);
    CHECK(!no_time && no_time.error().place.line == m.nupar + 5 && no_time.error().place.source == "yesterday");
    put(par, values + "\n\nFile:lecar.odex\nWed Jun 30 21:49:08 1993\nmore\n");
    const xpp::Result<std::vector<double>> more = xpp::read_parameter_file(m, par);
    CHECK(!more && more.error().place.line == m.nupar + 6);
    std::string ic_text;
    for (int i = 0; i < m.node; i++) ic_text += i == m.node - 1 ? "nan?\n" : "0.5\n";
    put(ic, ic_text);
    const xpp::Result<std::vector<double>> ir = xpp::read_ic_file(m, ic);
    CHECK(!ir && ir.error().place.file == ic && ir.error().place.line == m.node && ir.error().place.source == "nan?");
    xpp::ok_or_show(xpp::load_xppaut_values(s, xpp::ValueFile::initial_conditions, ic));
    CHECK(snapshot(s) == kept);
    put(ic, "0.5\n");
    const xpp::Result<std::vector<double>> short_ic = xpp::read_ic_file(m, ic);
    CHECK(!short_ic && short_ic.error().place.line == 2 && short_ic.error().what.starts_with("the file ends here"));

    for (const char *f : {xs, xsx, own, par, ic}) remove(f);
    TEST_REPORT("lunch round trip");
}
