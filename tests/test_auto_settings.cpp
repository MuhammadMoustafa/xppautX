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
#include "xpp_util.h"

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

    CHECK(ok(xpp::AUTO_NUM_COLLOCATION_POINTS, 2) && ok(xpp::AUTO_NUM_COLLOCATION_POINTS, 7));
    CHECK(!ok(xpp::AUTO_NUM_COLLOCATION_POINTS, 1) && !ok(xpp::AUTO_NUM_COLLOCATION_POINTS, 8));
    CHECK(!ok(xpp::AUTO_NUM_MESH_INTERVALS, 0) && !ok(xpp::AUTO_NUM_MESH_INTERVALS, 2.5) && ok(xpp::AUTO_NUM_MESH_INTERVALS, 50));
    CHECK(!ok(xpp::AUTO_NUM_MESH_INTERVALS, xpp::auto_max_mesh_intervals + 1));
    CHECK(ok(xpp::AUTO_NUM_MESH_INTERVALS, xpp::auto_max_mesh_intervals));
    CHECK(!ok(xpp::AUTO_NUM_STEP_SIZE, 0) && ok(xpp::AUTO_NUM_STEP_SIZE, -0.01));
    CHECK(!ok(xpp::AUTO_NUM_PARAMETER_TOLERANCE, 0) && ok(xpp::AUTO_NUM_PARAMETER_TOLERANCE, 1e-7));
    CHECK(ok(xpp::AUTO_NUM_PARAMETER_MIN, -1e6) && !ok(xpp::AUTO_NUM_PARAMETER_MIN, HUGE_VAL));
    CHECK(ok(xpp::AUTO_NUM_MAX_BRANCH_SWITCHES, 5) && !ok(xpp::AUTO_NUM_MAX_BRANCH_SWITCHES, -5));
    CHECK(ok(xpp::AUTO_NUM_SWITCH_ONE_DIRECTION, 1) && ok(xpp::AUTO_NUM_SUPPRESS_BRANCH_POINTS, 1) && !ok(xpp::AUTO_NUM_SUPPRESS_BRANCH_POINTS, 2));
    CHECK(ok(xpp::AUTO_NUM_OUTPUT_DETAIL, 5) && !ok(xpp::AUTO_NUM_OUTPUT_DETAIL, 6));
    CHECK(!ok(xpp::AUTO_NUM_MAX_STEPS, 3e9)); /* no int holds it */
    xpp::auto_settings_num_ok(xpp::AUTO_NUM_COLLOCATION_POINTS, 9, num_why);
    CHECK_STR(num_why.c_str(), "collocation_points=9: Collocation points must be a whole number from 2 to 7");
    CHECK_STR(xpp::auto_settings_num_key(xpp::AUTO_NUM_PARAMETER_MIN), "parameter_min");
    CHECK(xpp::auto_settings_num_key(xpp::AUTO_NUM_N) == nullptr);

    /* W531: a name's label is derived from it; a choice and a boolean are written and read by their words */
    CHECK_STR(xpp::name_label("max_branch_switches").c_str(), "Max branch switches");
    CHECK_STR(xpp::auto_settings_num_text(xpp::AUTO_NUM_OUTPUT_DETAIL, 3).c_str(), "start_jacobian");
    CHECK_STR(xpp::auto_settings_num_text(xpp::AUTO_NUM_SWITCH_ONE_DIRECTION, 1).c_str(), "true");
    CHECK_STR(xpp::auto_settings_num_text(xpp::AUTO_NUM_STEP_SIZE, -0.01).c_str(), "-0.01");
    const auto level = xpp::auto_settings_num_parse(xpp::AUTO_NUM_OUTPUT_DETAIL, "debugging");
    CHECK(level && *level == 5);
    const auto number = xpp::auto_settings_num_parse(xpp::AUTO_NUM_OUTPUT_DETAIL, "5");
    CHECK(!number && number.error().what.find("must be one of none, little, normal, start_jacobian, extensive, debugging") != std::string::npos);
    CHECK(!xpp::auto_settings_num_parse(xpp::AUTO_NUM_SUPPRESS_BRANCH_POINTS, "1") && !xpp::auto_settings_num_parse(xpp::AUTO_NUM_MESH_INTERVALS, "x"));
    const auto yes = xpp::auto_settings_num_parse(xpp::AUTO_NUM_SUPPRESS_BRANCH_POINTS, "true");
    CHECK(yes && *yes == 1);

    /* all or nothing: one bad value keeps the good one out */
    xpp::AutoSettingsSet s;
    s.has_num[xpp::AUTO_NUM_MAX_STEPS] = 1;
    s.num[xpp::AUTO_NUM_MAX_STEPS] = 30;
    s.has_num[xpp::AUTO_NUM_COLLOCATION_POINTS] = 1;
    s.num[xpp::AUTO_NUM_COLLOCATION_POINTS] = 9;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.nmx == 200 && xpp::client_session().auto_state.bifur.ncol == 4);
    s.num[xpp::AUTO_NUM_COLLOCATION_POINTS] = 5;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == 0 && xpp::client_session().auto_state.bifur.nmx == 30 && xpp::client_session().auto_state.bifur.ncol == 5);

    /* pairs in order, only checked when one of them is given */
    s = xpp::AutoSettingsSet{};
    s.has_num[xpp::AUTO_NUM_PARAMETER_MAX] = 1;
    s.num[xpp::AUTO_NUM_PARAMETER_MAX] = -1;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.rl1 == 2);
    CHECK(why.starts_with("Parameter min must be below Parameter max (parameter_min="));

    /* Dsmin <= |Ds| <= Dsmax, checked when one of the three is given (T23) */
    xpp::client_session().auto_state.bifur.ds = 0.02;
    s = xpp::AutoSettingsSet{};
    s.has_num[xpp::AUTO_NUM_STEP_SIZE] = 1;
    s.num[xpp::AUTO_NUM_STEP_SIZE] = -0.6;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.ds == 0.02);
    CHECK(why.starts_with("The size of Step size must be from Min step size to Max step size (its sign is the direction) (step_size=-0.6,"));
    s.num[xpp::AUTO_NUM_STEP_SIZE] = -0.5;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == 0 && xpp::client_session().auto_state.bifur.ds == -0.5);
    s = xpp::AutoSettingsSet{};
    s.has_num[xpp::AUTO_NUM_MIN_STEP_SIZE] = 1;
    s.num[xpp::AUTO_NUM_MIN_STEP_SIZE] = 0.6; /* above |Ds| and Dsmax */
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.bifur.dsmin == 0.001);

    /* axes: names among AUTO's parameters, ranges in order */
    s = xpp::AutoSettingsSet{};
    s.par1 = "GCA";
    s.var = "w";
    s.plot = xpp::AutoProjection::norm;
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == 0 && xpp::client_session().auto_state.axes().icp1 == 1 && xpp::client_session().auto_state.axes().var == 1 && xpp::client_session().auto_state.axes().plot == xpp::AutoProjection::norm);
    s.par1 = "nosuch";
    CHECK(xpp::auto_settings_apply(xpp::client_session(), s, why) == -1 && xpp::client_session().auto_state.axes().icp1 == 1);
    /* a projection is a name: XPPAUT's numbers are read on import only */
    CHECK(xpp::auto_projection_from_name("norm") == xpp::AutoProjection::norm && !xpp::auto_projection_from_name("1"));
    CHECK(xpp::auto_projection_from_xppaut(11) == xpp::AutoProjection::average && !xpp::auto_projection_from_xppaut(5));
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
