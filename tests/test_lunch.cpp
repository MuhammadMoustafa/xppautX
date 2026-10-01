/* The .set file round trip: write_lunch() then load_set_file() must bring
   back every parameter, initial condition and numerics setting, and
   writing again must give the same file. A field that one side writes and
   the other reads in a different order shifts every value after it, which
   a user only notices as a restored session that behaves differently.

   A file cut short or with a line that is not a number is refused, the
   line named (W116: no zeros for what is not there). Loading one is all or
   nothing (W125): a set file, a parameter file or an initial-conditions
   file whose last value is bad leaves the session exactly as it was (the
   set file it writes, the same), and its error names the file and that
   line.

   The model is loaded the way xppautX -silent loads it (xpp::load_model),
   without integrating. make test runs this from the top of the tree. */
#include "xpptest.h"
#include "session.h"
#include "lunch-new.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "expr.h"
#include "browse.h"
#include "graphics.h"
#include "load_eqn.h"
#include <stdio.h>
#include <string>

namespace {

/* path's bytes, "" when it cannot be read */
std::string bytes_of(const char *path)
{
    std::string b;
    return xpp::read_bytes(path, b) ? b : std::string();
}

/* the file without its first line, which carries the time it was written */
std::string body(const char *path)
{
    const std::string b = bytes_of(path);
    const std::size_t nl = b.find('\n');
    return nl == std::string::npos ? b : b.substr(nl + 1);
}

void put(const char *path, const std::string &text)
{
    xpp::Writer w = xpp::Writer::binary(path);
    CHECK(w && w.write(text) && w.commit());
}

/* load_set_file of text (written to path): the error, "" when it loads */
std::string read_error(const char *path, const std::string &text)
{
    put(path, text);
    const xpp::Result<> r = xpp::load_set_file(xpp::client_session(), path, false);
    return r ? std::string() : r.error().text(); /* "path:N: what" */
}

void save(const char *path)
{
    xpp::Writer w(path);
    xpp::write_lunch(xpp::client_session(), w.file());
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
    char arg0[] = "test_lunch", arg1[] = "examples/ode/lecar.ode";
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

    CHECK(xpp::load_set_file(s, a, false).has_value());

    xpp::get_val(s, "iapp", &x);
    CHECK(x == iapp);
    CHECK(s.last_ic[0] == v0);
    CHECK(s.numerics.tend == tend);
    CHECK(s.numerics.delta_t == dt);

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
    /* the line after the last one read: a set file holds its equations
       there, nothing else */
    const std::string extra = whole.substr(0, whole.find("RHS etc ...")) + "something else\n";
    CHECK(read_error(c, extra).starts_with(xpp::format("{}:{}: \"something else\"", c, bvp_high + 1)));

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

    for (const char *f : {a, b, c, par, ic}) remove(f);
    TEST_REPORT("lunch round trip");
}
