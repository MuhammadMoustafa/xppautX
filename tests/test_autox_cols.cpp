/* autox_screen_col(): autoX's printed column headings in the user's own names.
   The headings are exactly 14 characters wide and the substitution has to
   keep them so, or every row of the table below them lands in the wrong
   column. Nothing end-to-end would catch that: the numbers stay right and
   only the heading slides. */
#include "xpptest.h"
#include "session.h"
#include "autox_state.h"
#include "autox_nox.h"
#include "autox_c.h"
#include "model.h"
#include <string>

/* what the core holds for a loaded model; the names come from the .ode file */

static void load_model(void)
{
    xpp::client_session().size_variables(2);
    xpp::client_session().model().size_parameters(3);
    xpp::client_session().model().upar_names[0] = "iapp";
    xpp::client_session().model().upar_names[1] = "gca";
    xpp::client_session().model().upar_names[2] = "phi";
    xpp::client_session().model().uvar_names[0] = "v";
    xpp::client_session().model().uvar_names[1] = "w";
    xpp::client_session().model().node = xpp::client_session().model().neq = 2;
    xpp::client_session().model().nupar = 3;
    xpp::client_session().autox_state->parameter_layout.model_parameters=3;
}

TEST_CASE("test_autox_cols")
{
    std::string out;
    load_model();

    /* a parameter column becomes the parameter's name */
    out = autox_screen_col(xpp::client_session(), xpp::format("PAR({})",xpp::client_session().autox_state->parameter_layout.solver_index(1)));
    CHECK(out.size() == autox_column_width);
    CHECK(out.find("gca") != std::string::npos);

    out = autox_screen_col(xpp::client_session(), xpp::format("PAR({})",xpp::client_session().autox_state->parameter_layout.solver_index(0)));
    CHECK_STR(out.c_str(), "     iapp     ");

    /* a variable column, with and without the prefix autoX puts in front */
    xpp::client_session().model().uvar_names[0] = "v";
    out = autox_screen_col(xpp::client_session(), "     U(1)     ");
    CHECK_STR(out.c_str(), "      v       ");
    out = autox_screen_col(xpp::client_session(), "   MAX U(2)   ");
    CHECK(out.size() == autox_column_width);
    CHECK(out.find("MAX w") != std::string::npos);

    /* autoX's own quantities are not the user's parameters: leave them */
    out = autox_screen_col(xpp::client_session(), "    PERIOD    ");
    CHECK_STR(out.c_str(), "    PERIOD    ");
    out = autox_screen_col(xpp::client_session(), "   L2-NORM    ");
    CHECK_STR(out.c_str(), "   L2-NORM    ");
    /* PAR(10) is the period, whatever sits at parameter 10 */
    out = autox_screen_col(xpp::client_session(), "   PAR(10)    ");
    CHECK(out.find("T") != std::string::npos);

    /* a periodic branch prints MAX(n)/MIN(n): autoX has overwritten the U */
    xpp::client_session().model().uvar_names[1] = "w";
    out = autox_screen_col(xpp::client_session(), "   MAX(2)     ");
    CHECK(out.size() == autox_column_width);
    CHECK(out.find("MAX w") != std::string::npos);
    out = autox_screen_col(xpp::client_session(), "   MIN(1)     ");
    CHECK(out.find("MIN v") != std::string::npos);

    /* out of range: a variable autoX reports that this model does not have */
    out = autox_screen_col(xpp::client_session(), "     U(9)     ");
    CHECK_STR(out.c_str(), "     U(9)     ");

    /* a name as long as XPP allows must still not widen the column */
    xpp::client_session().model().uvar_names[0] = "abcdefghijk";
    out = autox_screen_col(xpp::client_session(), "   MAX U(1)   ");
    CHECK(out.size() == autox_column_width);

    TEST_REPORT("autox columns");
}
