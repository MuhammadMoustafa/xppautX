/* The set format's round trip: write_lunch() (a session's model.set) with
   XPPAUT's equations trailer after it, then import_xppaut_set() (File >
   Import XPPAUT set) must bring back every parameter, initial condition
   and numerics setting, and writing again must give the same file. A field that one side writes and
   the other reads in a different order shifts every value after it, which
   a user only notices as a restored session that behaves differently.

   A file cut short or with a line that is not a number is refused, the
   line named (W116: no zeros for what is not there). Loading one is all or
   nothing (W125): a set file, a parameter file or an initial-conditions
   file whose last value is bad leaves the session exactly as it was (the
   set file it writes, the same), and its error names the file and that
   line. A numerics value the setting's rule refuses (0 nout, 0 DeltaT)
   is refused as a bad number is. A session's set file ends at its last
   value; XPPAUT's, the import, has its model's equations after it, "RHS
   etc ..." (not read), and is refused without them. A parameter file's trailer
   is "File:" and the model, then the time as ctime writes it.

   The model is loaded the way xppautX --silent loads it (xpp::load_model),
   without integrating. make test runs this from the top of the tree. */
#include "xpptest.h"
#include "session.h"
#include "solver.h"
#include "lunch-new.h"
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
#include <filesystem>

namespace {

/* path's bytes, "" when it cannot be read */
std::string bytes_of(const char *path)
{
    std::string b;
    return xpp::read_bytes(path, b) ? b : std::string();
}

const std::string trailer = "RHS etc ...\n"; /* what XPPAUT's set file ends with */

/* the file without its first line, which carries the time it was written,
   and without the trailer */
std::string body(const char *path)
{
    std::string b = bytes_of(path);
    std::erase(b, '\r'); /* a text file: CRLF on Windows */
    const std::size_t nl = b.find('\n');
    if (nl != std::string::npos) b.erase(0, nl + 1);
    if (b.ends_with(trailer)) b.erase(b.size() - trailer.size());
    return b;
}

void put(const char *path, const std::string &text)
{
    xpp::Writer w = xpp::Writer::binary(path);
    CHECK(w && w.write(text) && w.commit());
}

/* import_xppaut_set of text (written to path): the error, "" when it loads */
std::string read_error(const char *path, const std::string &text)
{
    put(path, text);
    const xpp::Result<> r = xpp::import_xppaut_set(xpp::client_session(), path, false);
    return r ? std::string() : r.error().text(); /* "path:N: what" */
}

void save(const char *path)
{
    xpp::Writer w(path);
    xpp::write_lunch(xpp::client_session(), w.file());
    w.print("{}", trailer);
    CHECK(w.commit());
}

/* text with its line n (from 1) replaced by line */
std::string with_line(const std::string &text, int n, const std::string &line)
{
    std::size_t at = 0;
    for (int k = 1; k < n; k++) at = text.find('\n', at) + 1;
    return text.substr(0, at) + line + text.substr(text.find('\n', at));
}

/* the number (from 1) of the last line of text that ends with what */
int line_ending(const std::string &text, const std::string &what)
{
    int n = 0, found = 0;
    for (const std::string_view line : xpp::split_lines(text)) {
        n++;
        if (line.ends_with(what)) found = n;
    }
    return found;
}

} // namespace

int main(void)
{
    xpp::XppUi test_ui{};
    test_ui.save_replace = []() -> int { return xpp::SAVE_REPLACE; };
    xpp::set_ui(&test_ui); /* these writes target this test's scratch files */
    char arg0[] = "test_lunch", arg1[] = "examples/ode/lecar.odex";
    char *argv[] = {arg0, arg1, NULL};
    const char *a = "build/test_lunch_a.set", *b = "build/test_lunch_b.set", *c = "build/test_lunch_c.set";
    double iapp, v0, tend, dt, x;

    CHECK(xpp::load_model(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    init_browser(s);
    init_all_graph(s);

    xpp::get_val(s, "iapp", &iapp);
    v0 = s.last_ic[0];
    tend = s.numerics.tend;
    dt = s.numerics.delta_t;
    save(a);

    xpp::set_val(s, "iapp", iapp + 1);
    s.last_ic[0] = v0 + 1;
    s.numerics.tend = tend * 2;
    s.numerics.delta_t = dt / 2;

    CHECK(xpp::import_xppaut_set(s, a, false).has_value());

    xpp::get_val(s, "iapp", &x);
    CHECK(x == iapp);
    CHECK(s.last_ic[0] == v0);
    CHECK(s.numerics.tend == tend);
    CHECK(s.numerics.delta_t == dt);

    CHECK(s.saved_session.file == "build/test_lunch_a.snapx");
    CHECK(xpp::files::exists("build/test_lunch_a.snapx"));
    save(b);
    const std::string sa = body(a), sb = body(b);
    CHECK(sa.size() > 100);
    CHECK(sa == sb);
    /* cut short after its fifth line; its fifth line not a number */
    const std::string whole = "## Set file\n" + sa;
    std::size_t fifth = 0; /* where the fifth line begins */
    for (int k = 0; k < 4; k++) fifth = whole.find('\n', fifth) + 1;
    const std::string cut = whole.substr(0, whole.find('\n', fifth) + 1);
    const std::string short_error = read_error(b, cut);
    CHECK(short_error.starts_with(std::string(b) + ":6: the file ends here"));
    std::string bad = whole;
    bad.replace(fifth, 1, "x");
    const std::string bad_error = read_error(b, bad);
    CHECK(bad_error.starts_with(std::string(b) + ":5: \"x") && bad_error.find("is not a whole number (nout)") != std::string::npos);

    /* all or nothing: the session changed, then a set file whose last
       value (the BVP range's high end) does not read is refused at that
       line, and the session is as it was, value for value */
    xpp::set_val(s, "iapp", iapp + 2);
    s.last_ic[0] = v0 + 2;
    s.numerics.tend = tend * 3;
    save(b);
    const int bvp_high = line_ending(whole, "BVP range high");
    CHECK(bvp_high > 100);
    const std::string last_bad = with_line(whole, bvp_high, "1e999  BVP range high");
    const std::string last_error = read_error(c, last_bad);
    CHECK(last_error.starts_with(xpp::format("{}:{}: \"1e999  BVP range high\" is not a number", c, bvp_high)));
    save(a);
    CHECK(body(a) == body(b));
    xpp::get_val(s, "iapp", &x);
    CHECK(x == iapp + 2 && s.last_ic[0] == v0 + 2 && s.numerics.tend == tend * 3);
    /* the line after the last one read: XPPAUT's set file holds its
       equations there, nothing else; a session's holds nothing */
    CHECK(whole.find("RHS etc") == std::string::npos);
    const std::string extra = whole + "something else\n";
    CHECK(read_error(c, extra).starts_with(xpp::format("{}:{}: \"something else\"", c, bvp_high + 1)));
    CHECK(read_error(c, whole + trailer + "dV/dT=whatever\n").empty());
    /* a session's set file (no trailer) is not XPPAUT's: refused at its end */
    const std::string no_trailer = read_error(c, whole);
    CHECK(no_trailer.starts_with(xpp::format("{}:{}: the file ends here", c, bvp_high + 1)));
    CHECK(xpp::import_xppaut_set(s, b, false).has_value()); /* the session before that load again */
    const xpp::Result<xpp::SetFile> session_set = xpp::read_session_set(s, c, whole + "RHS etc ...\n");
    CHECK(!session_set && session_set.error().place.line == bvp_high + 1);
    CHECK(xpp::read_session_set(s, c, whole).has_value());
    /* a numerics value its rule refuses, at its line, nothing applied */
    for (const auto &[label, value, why] : {std::tuple{" nout", "0", "nOutput must be a whole number of at least 1"},
                                            std::tuple{"DeltaT", "0", "Dt must be a number other than 0"},
                                            std::tuple{"Bound", "-1", "Bounds must be a number above 0"},
                                            std::tuple{"Max Delay", "-1", "Maximal delay must be a number of at least 0"}}) {
        const int n = line_ending(whole, label);
        const std::string e = read_error(c, with_line(whole, n, xpp::format("{}  {}", value, label)));
        CHECK(e.starts_with(xpp::format("{}:{}: {}", c, n, why)));
        if (!e.starts_with(xpp::format("{}:{}: {}", c, n, why))) printf("  %s\n", e.c_str());
    }
    save(a);
    CHECK(body(a) == body(b));

    /* W132: set import checks suitability before applying any setting. */
    {
        const int line = line_ending(whole, "Runge-Kutta");
        const int before = s.numerics.method;
        const double before_total = s.numerics.tend;
        const int dimension = s.model().node;
        s.model().node = dimension + 1; /* the same import into an odd model */
        const std::string error = read_error(c, with_line(whole, line, xpp::format("{} Symplectic", static_cast<int>(xpp::method::SYMPLECT))));
        CHECK(error.starts_with(xpp::format("{}:{}:", c, line)));
        CHECK(error.find("even dimensions") != std::string::npos);
        CHECK(s.numerics.method == before && s.numerics.tend == before_total);
        s.model().node = dimension;
        const std::string unknown = read_error(c, with_line(whole, line, "99 Unknown"));
        CHECK(unknown.starts_with(xpp::format("{}:{}:", c, line)));
        CHECK(unknown.find("99 is not a method's number") != std::string::npos);
        CHECK(s.numerics.method == before);
    }

    /* Every named number is checked: both IC blocks, parameters, torus
       (including auxiliaries), fixed labels, and the value-dependent labels.
       A missing, reordered, differently cased or very long name is refused
       with its source line, before even an earlier valid change applies. */
    s.numerics.torus = 1;
    save(b);
    const std::string named = "## Set file\n" + body(b);
    s.numerics.tend *= 2; /* an earlier valid value must not apply on a later mismatch */
    save(b);
    const auto named_lines = xpp::split_lines(named);
    for (std::size_t k = 1; k < named_lines.size(); k++) {
        const std::string_view line = xpp::trim_blanks(named_lines[k]);
        if (line.empty() || !(line.front() == '-' || (line.front() >= '0' && line.front() <= '9'))) continue;
        const std::size_t end = line.find_first_of(" \t");
        if (end == std::string_view::npos || xpp::trim_blanks(line.substr(end)).empty()) continue;
        const std::string wrong = std::string(line.substr(0,end)) + "  another_model_name";
        put(c, with_line(named, static_cast<int>(k)+1, wrong) + trailer);
        const xpp::Result<> r = xpp::import_xppaut_set(s,c,false);
        CHECK(!r && r.error().place.file == c && r.error().place.line == static_cast<int>(k)+1
              && r.error().place.source == wrong && r.error().what.find("another_model_name") != std::string::npos);
        save(a);
        CHECK(body(a) == body(b));
    }
    const int par_line = line_ending(named, "  iapp");
    for (const std::string &name : {std::string(), std::string("IAPP"), std::string("gca"), std::string(10000,'q')}) {
        const std::string wrong = "17  " + name;
        put(c,with_line(named,par_line,wrong)+trailer);
        const xpp::Result<> r = xpp::import_xppaut_set(s,c,false);
        CHECK(!r && r.error().place.line == par_line && r.error().place.source == wrong);
        save(a);
        CHECK(body(a) == body(b));
    }
    /* A valid import whose adjacent session cannot be written keeps its
       applied values, returns one contextual error, and keeps the old identity. */
    const std::string saved_name = s.saved_session.file;
    const std::string blocked = "build/test_lunch_blocked.snapx";
    CHECK(std::filesystem::create_directory(blocked));
    put("build/test_lunch_blocked.set",with_line(named,par_line,"17  iapp")+trailer);
    const xpp::Result<> failed_save = xpp::import_xppaut_set(s,"build/test_lunch_blocked.set",false);
    CHECK(!failed_save && failed_save.error().what.find("saving session") != std::string::npos
          && failed_save.error().what.find("remain applied") != std::string::npos);
    xpp::get_val(s,"iapp",&x);
    CHECK(x == 17 && s.saved_session.file == saved_name);
    CHECK(std::filesystem::remove(blocked));
    remove("build/test_lunch_blocked.set");
    CHECK(xpp::import_xppaut_set(s,b,false).has_value());

    /* a parameter file and an initial-conditions file: the same */
    const xpp::Model &m = s.model();
    const char *par = "build/test_lunch.par", *ic = "build/test_lunch.ic";
    std::string par_text = xpp::format("{}   Number params\n", m.nupar);
    for (int i = 0; i < m.nupar; i++) par_text += xpp::format("{}  {}\n", i == m.nupar - 1 ? "x" : "1", m.upar_names[i]);
    put(par, par_text);
    const xpp::Result<std::vector<double>> pr = xpp::read_parameter_file(m, par);
    CHECK(!pr && pr.error().text() == xpp::format("{}:{}: \"x  {}\" is not a number ({})", par, m.nupar + 1,
                                                  m.upar_names[m.nupar - 1], m.upar_names[m.nupar - 1]));
    xpp::load_parameter_file_named(s, par);
    save(a);
    CHECK(body(a) == body(b));
    /* its trailer: as write_parameter_file writes it, then the end */
    remove(par); /* written afresh, no question asked */
    xpp::write_parameter_file(s, par);
    CHECK(xpp::read_parameter_file(m, par).has_value());
    std::string written = bytes_of(par);
    std::erase(written, '\r'); /* a text file: CRLF on Windows */
    const std::string values = written.substr(0, written.find("\n\nFile:"));
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
    xpp::load_ic_file_named(s, ic);
    save(a);
    CHECK(body(a) == body(b));
    put(ic, "0.5\n");
    const xpp::Result<std::vector<double>> short_ic = xpp::read_ic_file(m, ic);
    CHECK(!short_ic && short_ic.error().place.line == 2 && short_ic.error().what.starts_with("the file ends here"));

    for (const char *f : {a, b, c, par, ic, "build/test_lunch_a.snapx", "build/test_lunch_b.snapx", "build/test_lunch_c.snapx"}) remove(f);
    TEST_REPORT("lunch round trip");
}
