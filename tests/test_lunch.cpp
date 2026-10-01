/* The .set file round trip: write_lunch() then read_lunch() must bring back
   every parameter, initial condition and numerics setting, and writing again
   must give the same file. A field that one side writes and the other reads
   in a different order shifts every value after it, which a user only
   notices as a restored session that behaves differently.

   A file cut short or with a line that is not a number is refused, the
   line named (W116: no zeros for what is not there).

   The model is loaded the way xppautX -silent loads it (xpp::load_model),
   without integrating. make test runs this from the top of the tree. */
#include "xpptest.h"
#include "session.h"
#include "lunch-new.h"
#include "xpp_batch.h"
#include "expr.h"
#include "browse.h"
#include "graphics.h"
#include "load_eqn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>


/* the file without its first line, which carries the time it was written */
static char *body(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long n;
    char *s, *nl;
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    rewind(fp);
    s = static_cast<char *>(calloc(static_cast<size_t>(n) + 1, 1));
    if (fread(s, 1, static_cast<size_t>(n), fp) != static_cast<size_t>(n)) n = 0;
    fclose(fp);
    nl = strchr(s, '\n');
    return nl ? static_cast<char *>(memmove(s, nl + 1, strlen(nl + 1) + 1)) : s;
}

/* read_lunch of text (written to path): the error, "" when it reads */
static std::string read_error(const char *path, const std::string &text)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return "cannot write " + std::string(path);
    fwrite(text.data(), 1, text.size(), fp);
    fclose(fp);
    fp = fopen(path, "rb");
    if (!fp) return "cannot read " + std::string(path);
    const xpp::Result<> r = xpp::read_lunch(xpp::client_session(), fp, false);
    fclose(fp);
    return r ? std::string() : r.error().what;
}

static void save(const char *path)
{
    FILE *fp = fopen(path, "w");
    xpp::write_lunch(xpp::client_session(),fp);
    fclose(fp);
}

int main(void)
{
    char arg0[] = "test_lunch", arg1[] = "examples/ode/lecar.ode";
    char *argv[] = {arg0, arg1, NULL};
    const char *a = "build/test_lunch_a.set", *b = "build/test_lunch_b.set";
    double iapp, v0, tend, dt, x;
    FILE *fp;
    char *sa, *sb;

    CHECK(xpp::load_model(2, argv, 1).has_value());
    init_browser(xpp::client_session());
    init_all_graph(xpp::client_session());

    xpp::get_val(xpp::client_session(), "iapp", &iapp);
    v0 = xpp::client_session().last_ic[0];
    tend = xpp::client_session().numerics.tend;
    dt = xpp::client_session().numerics.delta_t;
    save(a);

    xpp::set_val(xpp::client_session(), "iapp", iapp + 1);
    xpp::client_session().last_ic[0] = v0 + 1;
    xpp::client_session().numerics.tend = tend * 2;
    xpp::client_session().numerics.delta_t = dt / 2;

    fp = fopen(a, "r");
    CHECK(fp != NULL);
    if (!fp) TEST_REPORT("lunch round trip");
    CHECK(xpp::read_lunch(xpp::client_session(), fp, false).has_value());
    fclose(fp);

    xpp::get_val(xpp::client_session(), "iapp", &x);
    CHECK(x == iapp);
    CHECK(xpp::client_session().last_ic[0] == v0);
    CHECK(xpp::client_session().numerics.tend == tend);
    CHECK(xpp::client_session().numerics.delta_t == dt);

    save(b);
    sa = body(a);
    sb = body(b);
    CHECK(sa && sb && strlen(sa) > 100);
    CHECK(sa && sb && strcmp(sa, sb) == 0);
    /* cut short after its fifth line; its fifth line not a number */
    const std::string whole = std::string("## Set file\n") + (sa ? sa : "");
    std::size_t fifth = 0; /* where the fifth line begins */
    for (int k = 0; k < 4; k++) fifth = whole.find('\n', fifth) + 1;
    const std::string cut = whole.substr(0, whole.find('\n', fifth) + 1);
    const std::string short_error = read_error(b, cut);
    CHECK(short_error.starts_with("line 6: the file ends here"));
    std::string bad = whole;
    bad.replace(fifth, 1, "x");
    const std::string bad_error = read_error(b, bad);
    CHECK(bad_error.starts_with("line 5: \"x") && bad_error.find("is not a whole number (nout)") != std::string::npos);
    free(sa);
    free(sb);
    remove(a);
    remove(b);
    TEST_REPORT("lunch round trip");
}
