/* Long names: a model's names have no length limit (W76), and the places
   that must fit one into a fixed width shorten it visibly instead of
   overflowing or silently cutting it to another name. The whole-model side
   (loading, the protocol, the forms, CSV and .set files, AUTO) is
   tools/autocheck.py's names section, with 200-character names. */
#include "xpptest.h"
#include "session.h"
#include "model.h"
#include <string>
#include "expr.h"
#include "auto_nox.h"
#include "lunch-new.h"
#include "xpp_util.h"
#include <stdio.h>

static double calc(const std::string &expr, int *ok)
{
    int command[256], length = 0;
    *ok = (add_expr(expr.c_str(), command, &length) == 0);
    return *ok ? evaluate(command) : 0.0;
}

int main(void)
{
    const std::string p200 = std::string(199, 'p') + "1", q1000(1000, 'q'), v200(200, 'v');
    std::string out, name;
    double z = 0;
    int ok, where = 0;

    xpp::Session &s = xpp::session();
    init_rpn(s);

    /* the parser keeps the whole name: before, it cut every name to 10
       characters, so two names that began alike were one symbol */
    CHECK(add_con(s, "stimulus_amplitude_first", 1.5) == 0);
    CHECK(add_con(s, "stimulus_amplitude_second", 2.5) == 0);
    CHECK(get_val("stimulus_amplitude_first", &z) && z == 1.5);
    CHECK(get_val("STIMULUS_AMPLITUDE_SECOND", &z) && z == 2.5); /* any case */
    CHECK(calc("stimulus_amplitude_second-stimulus_amplitude_first", &ok) == 1.0 && ok);

    /* no length limit: 200 and 1000 characters are names like any other */
    CHECK(add_con(s, p200.c_str(), 3.0) == 0);
    CHECK(get_val(p200, &z) && z == 3.0);
    CHECK(add_con(s, q1000.c_str(), 4.0) == 0);
    CHECK(get_val(q1000, &z) && z == 4.0);
    /* a longer name does not find the name it starts with, nor a shorter */
    CHECK(!get_val(p200 + "x", &z));
    CHECK(!get_val(p200.substr(0, 199), &z));
    /* a formula of long names (over 1200 characters) compiles whole */
    CHECK(calc(q1000 + "-" + p200, &ok) == 1.0 && ok);

    /* a long variable gets its primed name X' too */
    CHECK(add_var(xpp::session(), v200.c_str(), 0.0) == 0);
    CHECK(add_var(xpp::session(), (v200 + "'").c_str(), 0.0) == 0);

    /* "name:formula" hands back the name whole */
    CHECK(has_eq(p200 + ":2*3", name, &where) == 1);
    CHECK(name == p200 && where == 201);
    CHECK(has_eq("no formula", name, &where) == 0);

    /* short_name: for display only, with a marker when it shortens */
    CHECK_STR(short_name("gca", 10).c_str(), "gca");
    CHECK_STR(short_name("abcdefghij", 10).c_str(), "abcdefghij");
    CHECK_STR(short_name("abcdefghijk", 10).c_str(), "abcdefghi~");
    CHECK(short_name(q1000, 10) == std::string(9, 'q') + "~");

    /* AUTO's headings stay 14 wide: a long name is shortened with the marker
       and leaves a blank before the next heading */
    xpp::model().upar_names[0] = "applied_stimulus_current_amplitude";
    xpp::model().uvar_names[0] = "MEMBRANE_POTENTIAL_FAST_VARIABLE";
    xpp::model().node = xpp::model().neq = 1;
    xpp::model().nupar = 1;
    xpp::session().auto_state.npar = 1;
    xpp::session().auto_state.par[0] = 0;
    out = auto_screen_col("   PAR(0)     ");
    CHECK_STR(out.c_str(), "applied_stim~ ");
    out = auto_screen_col("   MAX U(1)   ");
    CHECK_STR(out.c_str(), "MAX MEMBRANE~ ");
    xpp::model().uvar_names[0] = "v";
    out = auto_screen_col("     U(1)     ");
    CHECK_STR(out.c_str(), "      v       "); /* a short name is centred as before */

    /* find_user_name finds a 200-character parameter, blanks and all */
    xpp::model().upar_names[0] = p200;
    CHECK(find_user_name(PARAMBOX, " " + p200.substr(0, 100) + " " + p200.substr(100)) == 0);
    CHECK(find_user_name(PARAMBOX, p200 + "x") == -1);

    /* a .set file line of a 1000-character name: read whole, and the next
       field is read from the next line */
    /* an anonymous temp file: the Windows and WSL test runs of one
       checkout may overlap, and a fixed name made them race */
    FILE *fp = tmpfile();
    CHECK(fp != NULL);
    if (fp) {
        std::string a, b;
        fprintf(fp, "%s\nnext\n", q1000.c_str());
        rewind(fp);
        io_string(a, fp, 1);
        io_string(b, fp, 1);
        CHECK(a == q1000);
        CHECK_STR(b.c_str(), "next");
        fclose(fp);
    }

    TEST_REPORT("long names");
}
