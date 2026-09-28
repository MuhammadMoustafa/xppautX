/* Long names: a model's names may be up to XPP_NAME_MAX characters, and the
   places that must fit one into a fixed width shorten it visibly instead of
   overflowing or silently cutting it to another name. The whole-model side
   (loading, the protocol, .set files, AUTO) is tools/autocheck.py's names
   section. */
#include "xpptest.h"
#include "session.h"
#include "model.h"
#include <string>
#include "expr.h"
#include "auto_nox.h"
#include "lunch-new.h"
#include "xpp_util.h"
#include <stdio.h>
#include <string.h>


static double calc(char *expr, int *ok)
{
    int command[256], length = 0;
    char buf[512];
    snprintf(buf, sizeof buf, "%s", expr);
    *ok = (add_expr(buf, command, &length) == 0);
    return *ok ? evaluate(command) : 0.0;
}

/* n copies of c */
static char *rep(char *s, int c, int n)
{
    memset(s, c, (size_t)n);
    s[n] = 0;
    return s;
}

int main(void)
{
    char p64[XPP_NAME_MAX + 2], p65[XPP_NAME_MAX + 2], v64[XPP_NAME_MAX + 2];
    char primed[XPP_NAME_MAX + 3], expr[400];
    std::string out;
    double z = 0;
    int ok;
    FILE *fp;

    init_rpn();

    /* the parser keeps the whole name: before, it cut every name to 10
       characters, so two names that began alike were one symbol */
    CHECK(add_con("stimulus_amplitude_first", 1.5) == 0);
    CHECK(add_con("stimulus_amplitude_second", 2.5) == 0);
    CHECK(get_val("stimulus_amplitude_first", &z) && z == 1.5);
    CHECK(get_val("STIMULUS_AMPLITUDE_SECOND", &z) && z == 2.5); /* any case */
    snprintf(expr, sizeof expr, "%s", "stimulus_amplitude_second-stimulus_amplitude_first");
    CHECK(calc(expr, &ok) == 1.0 && ok); /* calc takes a char * */

    /* XPP_NAME_MAX characters is a name, one more is refused */
    rep(p64, 'p', XPP_NAME_MAX);
    rep(p65, 'q', XPP_NAME_MAX + 1);
    CHECK(add_con(p64, 3.0) == 0);
    CHECK(get_val(p64, &z) && z == 3.0);
    CHECK(add_con(p65, 4.0) == 1);
    CHECK(!get_val(p65, &z));
    /* a longer name does not find the name it starts with */
    snprintf(expr, sizeof expr, "%sx", p64);
    CHECK(!get_val(expr, &z));

    /* a variable of the longest length still gets its primed name X' */
    rep(v64, 'v', XPP_NAME_MAX);
    CHECK(add_var(v64, 0.0) == 0);
    snprintf(primed, sizeof primed, "%s'", v64);
    CHECK(add_var(primed, 0.0) == 0);
    CHECK(name_too_long(v64) == 0);
    CHECK(name_too_long(p65) == 1);

    /* short_name: for display only, with a marker when it shortens */
    CHECK_STR(short_name("gca", 10).c_str(), "gca");
    CHECK_STR(short_name("abcdefghij", 10).c_str(), "abcdefghij");
    CHECK_STR(short_name("abcdefghijk", 10).c_str(), "abcdefghi~");

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

    /* a .set file line longer than a name: read whole, and the next field
       is read from the next line */
    fp = fopen("build/test_names.tmp", "w+");
    CHECK(fp != NULL);
    if (fp) {
        std::string a, b;
        fprintf(fp, "%s\nnext\n", p64);
        rewind(fp);
        io_string(a, fp, 1);
        io_string(b, fp, 1);
        CHECK_STR(a.c_str(), p64);
        CHECK_STR(b.c_str(), "next");
        fclose(fp);
        remove("build/test_names.tmp");
    }

    TEST_REPORT("long names");
}
