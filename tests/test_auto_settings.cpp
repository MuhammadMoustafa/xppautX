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
    xpp::client_session().size_variables(2);
    xpp::client_session().model().size_parameters(3);
    xpp::client_session().model().upar_names[0] = "iapp";
    xpp::client_session().model().upar_names[1] = "gca";
    xpp::client_session().model().upar_names[2] = "phi";
    xpp::client_session().model().uvar_names[0] = "v";
    xpp::client_session().model().uvar_names[1] = "w";
    xpp::client_session().model().node = xpp::client_session().model().neq = 2;
    xpp::client_session().model().nupar = 3;
    xpp::Model &m=xpp::client_session().model();
    for(int i=0;i<m.nupar;i++)m.define_name(m.upar_names[i],xpp::Model::NameKind::Param,i,{}).parameter=i;
    for(int i=0;i<m.neq;i++)m.define_name(m.uvar_names[i],xpp::Model::NameKind::Variable,i,{}).column=i;
    xpp::client_session().auto_state.bifur.nmx = 200;
    xpp::client_session().auto_state.bifur.ncol = 4;
    xpp::client_session().auto_state.bifur.dsmin = 0.001;
    xpp::client_session().auto_state.bifur.dsmax = 0.5;
    xpp::client_session().auto_state.bifur.rl0 = 0;
    xpp::client_session().auto_state.bifur.rl1 = 2;
    xpp::client_session().auto_state.axes().xmin = -1;
    xpp::client_session().auto_state.axes().xmax = 1;
    xpp::client_session().auto_state.axes().ymin = -1;
    xpp::client_session().auto_state.axes().ymax = 1;
    xpp::client_session().auto_state.axes().icp1 = 0;
    xpp::client_session().auto_state.axes().icp2 = 1;
}

bool ok(int field, double v)
{
    std::string why;
    return xpp::auto_settings_num_ok(field, v, why) != 0;
}

} // namespace

int main()
{
    std::string num_why;
    std::string why;
    load_model();

    CHECK(xpp::auto_min_mesh_intervals == 1 && xpp::auto_max_mesh_intervals == 10000);
    CHECK(xpp::auto_min_collocation_points == 2 && xpp::auto_max_collocation_points == 7);
    CHECK(xpp::auto_min_diagnostic_level == 0 && xpp::auto_max_diagnostic_level == 5);
    CHECK(ok(xpp::AUTO_NUM_NCOL, xpp::auto_min_collocation_points) && ok(xpp::AUTO_NUM_NCOL, xpp::auto_max_collocation_points));
    CHECK(!ok(xpp::AUTO_NUM_NCOL, xpp::auto_min_collocation_points - 1) && !ok(xpp::AUTO_NUM_NCOL, xpp::auto_max_collocation_points + 1));
    CHECK(!ok(xpp::AUTO_NUM_NTST, xpp::auto_min_mesh_intervals - 1) && !ok(xpp::AUTO_NUM_NTST, 2.5) && ok(xpp::AUTO_NUM_NTST, 50));
    CHECK(!ok(xpp::AUTO_NUM_NTST, xpp::auto_max_mesh_intervals + 1));
    CHECK(ok(xpp::AUTO_NUM_NTST, xpp::auto_max_mesh_intervals));

    const AutoOptions def_bifur{};
    CHECK(def_bifur.ntst == xpp::auto_default_ntst);
    CHECK(def_bifur.nmx == xpp::auto_default_nmx);
    CHECK(def_bifur.npr == xpp::auto_default_npr);
    CHECK(def_bifur.ncol == xpp::auto_default_ncol);
    CHECK(def_bifur.ds == xpp::auto_default_ds);
    CHECK(def_bifur.dsmax == xpp::auto_default_dsmax);
    CHECK(def_bifur.dsmin == xpp::auto_default_dsmin);
    CHECK(def_bifur.rl0 == xpp::auto_default_rl0);
    CHECK(def_bifur.rl1 == xpp::auto_default_rl1);
    CHECK(def_bifur.a0 == xpp::auto_default_a0);
    CHECK(def_bifur.a1 == xpp::auto_default_a1);

    const AutoAxes def_axes{};
    CHECK(def_axes.xmax == xpp::auto_default_xmax);
    CHECK(def_axes.xmin == xpp::auto_default_xmin);
    CHECK(def_axes.ymax == xpp::auto_default_ymax);
    CHECK(def_axes.ymin == xpp::auto_default_ymin);
    CHECK(def_axes.var == xpp::auto_default_var);

    const ADVAUTO def_adv{};
    CHECK(def_adv.epsl == xpp::auto_default_epsl);
    CHECK(def_adv.epsu == xpp::auto_default_epsu);
    CHECK(def_adv.epss == xpp::auto_default_epss);
    CHECK(def_adv.iad == xpp::auto_default_iad);
    CHECK(def_adv.mxbf == xpp::auto_default_mxbf);
    CHECK(def_adv.iid == xpp::auto_default_iid);
    CHECK(def_adv.itmx == xpp::auto_default_itmx);
    CHECK(def_adv.itnw == xpp::auto_default_itnw);
    CHECK(def_adv.nwtn == xpp::auto_default_nwtn);
    CHECK(def_adv.iads == xpp::auto_default_iads);
    CHECK(!ok(xpp::AUTO_NUM_DS, 0) && ok(xpp::AUTO_NUM_DS, -0.01));
    CHECK(!ok(xpp::AUTO_NUM_EPSL, 0) && ok(xpp::AUTO_NUM_EPSL, 1e-7));
    CHECK(ok(xpp::AUTO_NUM_RL0, -1e6) && !ok(xpp::AUTO_NUM_RL0, HUGE_VAL));
    CHECK(ok(xpp::AUTO_NUM_MXBF, -5) && ok(xpp::AUTO_NUM_SUPPBP, 1) && !ok(xpp::AUTO_NUM_SUPPBP, 2));
    CHECK(!ok(xpp::AUTO_NUM_NMX, 3e9)); /* no int holds it */
    xpp::auto_settings_num_ok(xpp::AUTO_NUM_NCOL, 9, num_why);
    CHECK_STR(num_why.c_str(), "Ncol must be a whole number from 2 to 7");
    CHECK_STR(xpp::auto_settings_num_key(xpp::AUTO_NUM_RL0), "rl0");
    CHECK_STR(xpp::auto_settings_num_label(xpp::AUTO_NUM_RL0), "Par Min");
    CHECK(xpp::auto_settings_num_key(xpp::AUTO_NUM_N) == nullptr);

    /* all or nothing: one bad value keeps the good one out */
    xpp::AutoSettingsSet s;
    s.has_num[xpp::AUTO_NUM_NMX] = 1;
    s.num[xpp::AUTO_NUM_NMX] = 30;
    s.has_num[xpp::AUTO_NUM_NCOL] = 1;
    s.num[xpp::AUTO_NUM_NCOL] = 9;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.nmx == 200 && xpp::client_session().auto_state.bifur.ncol == 4);
    s.num[xpp::AUTO_NUM_NCOL] = 5;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == 0 && xpp::client_session().auto_state.bifur.nmx == 30 && xpp::client_session().auto_state.bifur.ncol == 5);

    /* pairs in order, only checked when one of them is given */
    s = xpp::AutoSettingsSet{};
    s.has_num[xpp::AUTO_NUM_RL1] = 1;
    s.num[xpp::AUTO_NUM_RL1] = -1;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.rl1 == 2);
    CHECK_STR(why.c_str(), "Par Min must be below Par Max");

    /* Dsmin <= |Ds| <= Dsmax, checked when one of the three is given (T23) */
    xpp::client_session().auto_state.bifur.ds = 0.02;
    s = xpp::AutoSettingsSet{};
    s.has_num[xpp::AUTO_NUM_DS] = 1;
    s.num[xpp::AUTO_NUM_DS] = -0.6;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.ds == 0.02);
    CHECK_STR(why.c_str(), "Ds must be from Dsmin to Dsmax in size (its sign is the direction)");
    s.num[xpp::AUTO_NUM_DS] = -0.5;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == 0 && xpp::client_session().auto_state.bifur.ds == -0.5);
    s = xpp::AutoSettingsSet{};
    s.has_num[xpp::AUTO_NUM_DSMIN] = 1;
    s.num[xpp::AUTO_NUM_DSMIN] = 0.6; /* above |Ds| and Dsmax */
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.dsmin == 0.001);

    /* axes: names among AUTO's parameters, ranges in order */
    s = xpp::AutoSettingsSet{};
    s.par1 = "GCA";
    s.var = "w";
    s.has_plot = 1;
    s.plot = 1;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == 0 && xpp::client_session().auto_state.axes().icp1 == 1 && xpp::client_session().auto_state.axes().var == 1 && xpp::client_session().auto_state.axes().plot == 1);
    s.par1 = "nosuch";
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.axes().icp1 == 1);
    s = xpp::AutoSettingsSet{};
    s.has_plot = 1;
    s.plot = 5;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.axes().plot == 1);
    s = xpp::AutoSettingsSet{};
    s.has_range[2] = 1;
    s.range[2] = 3; /* ymin above ymax */
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.axes().ymin == -1);

    /* Mark values: a parameter of AUTO's or T, into AUTO's user points */
    s = xpp::AutoSettingsSet{};
    s.nmarks = 2;
    s.mark_name = {"phi","t"};
    s.mark_value = {0.5,20};
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == 0 && xpp::client_session().auto_state.bifur.marks.size() == 2);
    CHECK(xpp::client_session().auto_state.bifur.marks[0].parameter == 2 && xpp::client_session().auto_state.bifur.marks[0].value == 0.5 && xpp::client_session().auto_state.bifur.marks[1].parameter == 10 && xpp::client_session().auto_state.bifur.marks[1].value == 20);
    s.mark_name[1] = "v"; /* a variable is no user point */
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.marks[1].parameter == 10);

    TEST_REPORT("auto_settings");
}
