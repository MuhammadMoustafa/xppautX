/* Long names: a model's names may be up to XPP_NAME_MAX characters, and the
   places that must fit one into a fixed width shorten it visibly instead of
   overflowing or silently cutting it to another name. The whole-model side
   (loading, the protocol, .set files, AUTO) is tools/autocheck.py's names
   section. */
#include "xpptest.h"
#include "parserslow.h"
#include "auto_nox.h"
#include "lunch-new.h"
#include "xpp_util.h"
#include <stdio.h>
#include <string.h>

extern char upar_names[][XPP_NAME_MAX + 1];
extern char uvar_names[][XPP_NAME_MAX + 1];
extern int AutoPar[8];
extern int NAutoPar;
extern int NODE, NEQ;

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
    char primed[XPP_NAME_MAX + 3], expr[400], out[AUTO_COL_W + 1], s[16];
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
    CHECK(calc("stimulus_amplitude_second-stimulus_amplitude_first", &ok) == 1.0 && ok);

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
    short_name(s, "gca", 10);
    CHECK_STR(s, "gca");
    short_name(s, "abcdefghij", 10);
    CHECK_STR(s, "abcdefghij");
    short_name(s, "abcdefghijk", 10);
    CHECK_STR(s, "abcdefghi~");

    /* AUTO's headings stay 14 wide: a long name is shortened with the marker
       and leaves a blank before the next heading */
    strcpy(upar_names[0], "applied_stimulus_current_amplitude");
    strcpy(uvar_names[0], "MEMBRANE_POTENTIAL_FAST_VARIABLE");
    NODE = NEQ = 1;
    NAutoPar = 1;
    AutoPar[0] = 0;
    auto_screen_col("   PAR(0)     ", out);
    CHECK_STR(out, "applied_stim~ ");
    auto_screen_col("   MAX U(1)   ", out);
    CHECK_STR(out, "MAX MEMBRANE~ ");
    strcpy(uvar_names[0], "v");
    auto_screen_col("     U(1)     ", out);
    CHECK_STR(out, "      v       "); /* a short name is centred as before */

    /* a .set file line longer than the field: the field gets its start and
       the next field is read from the next line, not from the rest */
    fp = fopen("build/test_names.tmp", "w+");
    CHECK(fp != NULL);
    if (fp) {
        char a[11], b[11];
        fprintf(fp, "%s\nnext\n", p64);
        rewind(fp);
        io_string(a, sizeof a, fp, 1);
        io_string(b, sizeof b, fp, 1);
        CHECK_STR(a, "pppppppppp");
        CHECK_STR(b, "next");
        fclose(fp);
        remove("build/test_names.tmp");
    }

    TEST_REPORT("long names");
}
