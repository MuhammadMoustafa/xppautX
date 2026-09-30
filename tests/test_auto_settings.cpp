/* auto_settings: what an `auto` `set` accepts and that it sets all or
   nothing (docs/protocol.md "AUTO's settings as data"). A value AUTO takes
   badly gets through to a run otherwise: Ncol above 7 exits the program.
   tools/servercheck.py checks the same through the protocol, against the
   forms. */
#include "xpptest.h"
#include "session.h"
#include "model.h"
#include "auto_nox.h"
#include "auto_settings.h"

#include <cmath>
#include <cstring>

namespace {

void load_model()
{
    xpp::model().upar_names[0] = "iapp";
    xpp::model().upar_names[1] = "gca";
    xpp::model().upar_names[2] = "phi";
    xpp::model().uvar_names[0] = "v";
    xpp::model().uvar_names[1] = "w";
    xpp::model().node = xpp::model().neq = 2;
    xpp::model().nupar = 3;
    xpp::session().auto_state.npar = 3;
    for (int i = 0; i < 3; i++) xpp::session().auto_state.par[i] = i;
    xpp::session().auto_state.bifur.nmx = 200;
    xpp::session().auto_state.bifur.ncol = 4;
    xpp::session().auto_state.bifur.dsmin = 0.001;
    xpp::session().auto_state.bifur.dsmax = 0.5;
    xpp::session().auto_state.bifur.rl0 = 0;
    xpp::session().auto_state.bifur.rl1 = 2;
    xpp::session().auto_state.axes().xmin = -1;
    xpp::session().auto_state.axes().xmax = 1;
    xpp::session().auto_state.axes().ymin = -1;
    xpp::session().auto_state.axes().ymax = 1;
    xpp::session().auto_state.axes().icp1 = 0;
    xpp::session().auto_state.axes().icp2 = 1;
}

bool ok(int field, double v)
{
    std::string why;
    return auto_settings_num_ok(field, v, why) != 0;
}

} // namespace

int main()
{
    std::string num_why;
    std::string why;
    load_model();

    CHECK(ok(AUTO_NUM_NCOL, 2) && ok(AUTO_NUM_NCOL, 7));
    CHECK(!ok(AUTO_NUM_NCOL, 1) && !ok(AUTO_NUM_NCOL, 8));
    CHECK(!ok(AUTO_NUM_NTST, 0) && !ok(AUTO_NUM_NTST, 2.5) && ok(AUTO_NUM_NTST, 50));
    CHECK(!ok(AUTO_NUM_DS, 0) && ok(AUTO_NUM_DS, -0.01));
    CHECK(!ok(AUTO_NUM_EPSL, 0) && ok(AUTO_NUM_EPSL, 1e-7));
    CHECK(ok(AUTO_NUM_RL0, -1e6) && !ok(AUTO_NUM_RL0, HUGE_VAL));
    CHECK(ok(AUTO_NUM_MXBF, -5) && ok(AUTO_NUM_SUPPBP, 1) && !ok(AUTO_NUM_SUPPBP, 2));
    CHECK(!ok(AUTO_NUM_NMX, 3e9)); /* no int holds it */
    auto_settings_num_ok(AUTO_NUM_NCOL, 9, num_why);
    CHECK_STR(num_why.c_str(), "Ncol must be a whole number from 2 to 7");
    CHECK_STR(auto_settings_num_key(AUTO_NUM_RL0), "rl0");
    CHECK_STR(auto_settings_num_label(AUTO_NUM_RL0), "Par Min");
    CHECK(auto_settings_num_key(AUTO_NUM_N) == nullptr);

    /* all or nothing: one bad value keeps the good one out */
    AutoSettingsSet s;
    s.has_num[AUTO_NUM_NMX] = 1;
    s.num[AUTO_NUM_NMX] = 30;
    s.has_num[AUTO_NUM_NCOL] = 1;
    s.num[AUTO_NUM_NCOL] = 9;
    CHECK(auto_settings_apply(xpp::session(), s, why) == -1 && xpp::session().auto_state.bifur.nmx == 200 && xpp::session().auto_state.bifur.ncol == 4);
    s.num[AUTO_NUM_NCOL] = 5;
    CHECK(auto_settings_apply(xpp::session(), s, why) == 0 && xpp::session().auto_state.bifur.nmx == 30 && xpp::session().auto_state.bifur.ncol == 5);

    /* pairs in order, only checked when one of them is given */
    s = AutoSettingsSet{};
    s.has_num[AUTO_NUM_RL1] = 1;
    s.num[AUTO_NUM_RL1] = -1;
    CHECK(auto_settings_apply(xpp::session(), s, why) == -1 && xpp::session().auto_state.bifur.rl1 == 2);
    CHECK_STR(why.c_str(), "Par Min must be below Par Max");

    /* Dsmin <= |Ds| <= Dsmax, checked when one of the three is given (T23) */
    xpp::session().auto_state.bifur.ds = 0.02;
    s = AutoSettingsSet{};
    s.has_num[AUTO_NUM_DS] = 1;
    s.num[AUTO_NUM_DS] = -0.6;
    CHECK(auto_settings_apply(xpp::session(), s, why) == -1 && xpp::session().auto_state.bifur.ds == 0.02);
    CHECK_STR(why.c_str(), "Ds must be from Dsmin to Dsmax in size (its sign is the direction)");
    s.num[AUTO_NUM_DS] = -0.5;
    CHECK(auto_settings_apply(xpp::session(), s, why) == 0 && xpp::session().auto_state.bifur.ds == -0.5);
    s = AutoSettingsSet{};
    s.has_num[AUTO_NUM_DSMIN] = 1;
    s.num[AUTO_NUM_DSMIN] = 0.6; /* above |Ds| and Dsmax */
    CHECK(auto_settings_apply(xpp::session(), s, why) == -1 && xpp::session().auto_state.bifur.dsmin == 0.001);

    /* axes: names among AUTO's parameters, ranges in order */
    s = AutoSettingsSet{};
    s.par1 = "GCA";
    s.var = "w";
    s.has_plot = 1;
    s.plot = 1;
    CHECK(auto_settings_apply(xpp::session(), s, why) == 0 && xpp::session().auto_state.axes().icp1 == 1 && xpp::session().auto_state.axes().var == 1 && xpp::session().auto_state.axes().plot == 1);
    s.par1 = "nosuch";
    CHECK(auto_settings_apply(xpp::session(), s, why) == -1 && xpp::session().auto_state.axes().icp1 == 1);
    s = AutoSettingsSet{};
    s.has_plot = 1;
    s.plot = 5;
    CHECK(auto_settings_apply(xpp::session(), s, why) == -1 && xpp::session().auto_state.axes().plot == 1);
    s = AutoSettingsSet{};
    s.has_range[2] = 1;
    s.range[2] = 3; /* ymin above ymax */
    CHECK(auto_settings_apply(xpp::session(), s, why) == -1 && xpp::session().auto_state.axes().ymin == -1);

    /* Mark values: a parameter of AUTO's or T, into AUTO's user points */
    s = AutoSettingsSet{};
    s.nmarks = 2;
    s.mark_name[0] = "phi";
    s.mark_value[0] = 0.5;
    s.mark_name[1] = "t";
    s.mark_value[1] = 20;
    CHECK(auto_settings_apply(xpp::session(), s, why) == 0 && xpp::session().auto_state.bifur.nper == 2 && xpp::session().auto_state.nuzr == 2);
    CHECK(xpp::session().auto_state.uzr_par[0] == 2 && xpp::session().auto_state.uzr_period[0] == 0.5 && xpp::session().auto_state.uzr_par[1] == 10 && xpp::session().auto_state.uzr_period[1] == 20);
    s.mark_name[1] = "v"; /* a variable is no user point */
    CHECK(auto_settings_apply(xpp::session(), s, why) == -1 && xpp::session().auto_state.uzr_par[1] == 10);

    TEST_REPORT("auto_settings");
}
