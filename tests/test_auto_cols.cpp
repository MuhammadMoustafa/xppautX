/* auto_screen_col(): AUTO's printed column headings in the user's own names.
   The headings are exactly 14 characters wide and the substitution has to
   keep them so, or every row of the table below them lands in the wrong
   column. Nothing end-to-end would catch that: the numbers stay right and
   only the heading slides. */
#include "xpptest.h"
#include "auto_nox.h"
#include "model.h"

/* what the core holds for a loaded model; the names come from the .ode file */
extern int AutoPar[8];
extern int NAutoPar;
extern int NODE, NEQ, NUPAR;

static void load_model(void)
{
    xpp::model().upar_names[0] = "iapp";
    xpp::model().upar_names[1] = "gca";
    xpp::model().upar_names[2] = "phi";
    xpp::model().uvar_names[0] = "v";
    xpp::model().uvar_names[1] = "w";
    NODE = NEQ = 2;
    NUPAR = 3;
    NAutoPar = 3;
    AutoPar[0] = 0;
    AutoPar[1] = 1;
    AutoPar[2] = 2;
}

int main(void)
{
    char out[AUTO_COL_W + 1];
    load_model();

    /* a parameter column becomes the parameter's name */
    auto_screen_col("   PAR(1)     ", out);
    CHECK(strlen(out) == AUTO_COL_W);
    CHECK(strstr(out, "gca") != NULL);

    auto_screen_col("   PAR(0)     ", out);
    CHECK_STR(out, "     iapp     ");

    /* a variable column, with and without the prefix AUTO puts in front */
    xpp::model().uvar_names[0] = "v";
    auto_screen_col("     U(1)     ", out);
    CHECK_STR(out, "      v       ");
    auto_screen_col("   MAX U(2)   ", out);
    CHECK(strlen(out) == AUTO_COL_W);
    CHECK(strstr(out, "MAX w") != NULL);

    /* AUTO's own quantities are not the user's parameters: leave them */
    auto_screen_col("    PERIOD    ", out);
    CHECK_STR(out, "    PERIOD    ");
    auto_screen_col("   L2-NORM    ", out);
    CHECK_STR(out, "   L2-NORM    ");
    /* PAR(10) is the period, whatever sits at parameter 10 */
    auto_screen_col("   PAR(10)    ", out);
    CHECK(strstr(out, "T") != NULL);

    /* a periodic branch prints MAX(n)/MIN(n): AUTO has overwritten the U */
    xpp::model().uvar_names[1] = "w";
    auto_screen_col("   MAX(2)     ", out);
    CHECK(strlen(out) == AUTO_COL_W);
    CHECK(strstr(out, "MAX w") != NULL);
    auto_screen_col("   MIN(1)     ", out);
    CHECK(strstr(out, "MIN v") != NULL);

    /* out of range: a variable AUTO reports that this model does not have */
    auto_screen_col("     U(9)     ", out);
    CHECK_STR(out, "     U(9)     ");

    /* a name as long as XPP allows must still not widen the column */
    xpp::model().uvar_names[0] = "abcdefghijk";
    auto_screen_col("   MAX U(1)   ", out);
    CHECK(strlen(out) == AUTO_COL_W);

    TEST_REPORT("auto columns");
}
