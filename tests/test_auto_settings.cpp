/* auto_settings: what an `auto` `set` accepts and that it sets all or
   nothing (docs/protocol.md "AUTO's settings as data"). A value AUTO takes
   badly gets through to a run otherwise: Ncol above 7 exits the program.
   tools/servercheck.py checks the same through the protocol, against the
   forms. */
#include "xpptest.h"
#include "auto_nox.h"
#include "auto_settings.h"

#include <cmath>
#include <cstring>

extern "C" {
extern char upar_names[][XPP_NAME_MAX + 1];
extern char uvar_names[][XPP_NAME_MAX + 1];
extern int AutoPar[8];
extern int NAutoPar;
extern int NODE, NEQ, NUPAR;
extern BIFUR Auto;
extern double outperiod[20];
extern long UzrPar[20];
extern int NAutoUzr;
}

namespace {

void model()
{
    std::strcpy(upar_names[0], "iapp");
    std::strcpy(upar_names[1], "gca");
    std::strcpy(upar_names[2], "phi");
    std::strcpy(uvar_names[0], "v");
    std::strcpy(uvar_names[1], "w");
    NODE = NEQ = 2;
    NUPAR = 3;
    NAutoPar = 3;
    for (int i = 0; i < 3; i++) AutoPar[i] = i;
    Auto.nmx = 200;
    Auto.ncol = 4;
    Auto.dsmin = 0.001;
    Auto.dsmax = 0.5;
    Auto.rl0 = 0;
    Auto.rl1 = 2;
    Auto.xmin = -1;
    Auto.xmax = 1;
    Auto.ymin = -1;
    Auto.ymax = 1;
    Auto.icp1 = 0;
    Auto.icp2 = 1;
}

bool ok(int field, double v)
{
    char why[200];
    return auto_settings_num_ok(field, v, why, sizeof why) != 0;
}

} // namespace

int main()
{
    char why[200];
    model();

    CHECK(ok(AUTO_NUM_NCOL, 2) && ok(AUTO_NUM_NCOL, 7));
    CHECK(!ok(AUTO_NUM_NCOL, 1) && !ok(AUTO_NUM_NCOL, 8));
    CHECK(!ok(AUTO_NUM_NTST, 0) && !ok(AUTO_NUM_NTST, 2.5) && ok(AUTO_NUM_NTST, 50));
    CHECK(!ok(AUTO_NUM_DS, 0) && ok(AUTO_NUM_DS, -0.01));
    CHECK(!ok(AUTO_NUM_EPSL, 0) && ok(AUTO_NUM_EPSL, 1e-7));
    CHECK(ok(AUTO_NUM_RL0, -1e6) && !ok(AUTO_NUM_RL0, HUGE_VAL));
    CHECK(ok(AUTO_NUM_MXBF, -5) && ok(AUTO_NUM_SUPPBP, 1) && !ok(AUTO_NUM_SUPPBP, 2));
    CHECK(!ok(AUTO_NUM_NMX, 3e9)); /* no int holds it */
    auto_settings_num_ok(AUTO_NUM_NCOL, 9, why, sizeof why);
    CHECK_STR(why, "Ncol must be a whole number from 2 to 7");
    CHECK_STR(auto_settings_num_key(AUTO_NUM_RL0), "rl0");
    CHECK_STR(auto_settings_num_label(AUTO_NUM_RL0), "Par Min");
    CHECK(auto_settings_num_key(AUTO_NUM_N) == nullptr);

    /* all or nothing: one bad value keeps the good one out */
    AutoSettingsSet s;
    auto_settings_set_init(&s);
    s.has_num[AUTO_NUM_NMX] = 1;
    s.num[AUTO_NUM_NMX] = 30;
    s.has_num[AUTO_NUM_NCOL] = 1;
    s.num[AUTO_NUM_NCOL] = 9;
    CHECK(auto_settings_apply(&s, why, sizeof why) == -1 && Auto.nmx == 200 && Auto.ncol == 4);
    s.num[AUTO_NUM_NCOL] = 5;
    CHECK(auto_settings_apply(&s, why, sizeof why) == 0 && Auto.nmx == 30 && Auto.ncol == 5);

    /* pairs in order, only checked when one of them is given */
    auto_settings_set_init(&s);
    s.has_num[AUTO_NUM_RL1] = 1;
    s.num[AUTO_NUM_RL1] = -1;
    CHECK(auto_settings_apply(&s, why, sizeof why) == -1 && Auto.rl1 == 2);
    CHECK_STR(why, "Par Min must be below Par Max");

    /* Dsmin <= |Ds| <= Dsmax, checked when one of the three is given (T23) */
    Auto.ds = 0.02;
    auto_settings_set_init(&s);
    s.has_num[AUTO_NUM_DS] = 1;
    s.num[AUTO_NUM_DS] = -0.6;
    CHECK(auto_settings_apply(&s, why, sizeof why) == -1 && Auto.ds == 0.02);
    CHECK_STR(why, "Ds must be from Dsmin to Dsmax in size (its sign is the direction)");
    s.num[AUTO_NUM_DS] = -0.5;
    CHECK(auto_settings_apply(&s, why, sizeof why) == 0 && Auto.ds == -0.5);
    auto_settings_set_init(&s);
    s.has_num[AUTO_NUM_DSMIN] = 1;
    s.num[AUTO_NUM_DSMIN] = 0.6; /* above |Ds| and Dsmax */
    CHECK(auto_settings_apply(&s, why, sizeof why) == -1 && Auto.dsmin == 0.001);

    /* axes: names among AUTO's parameters, ranges in order */
    auto_settings_set_init(&s);
    std::strcpy(s.par1, "GCA");
    std::strcpy(s.var, "w");
    s.has_plot = 1;
    s.plot = 1;
    CHECK(auto_settings_apply(&s, why, sizeof why) == 0 && Auto.icp1 == 1 && Auto.var == 1 && Auto.plot == 1);
    std::strcpy(s.par1, "nosuch");
    CHECK(auto_settings_apply(&s, why, sizeof why) == -1 && Auto.icp1 == 1);
    auto_settings_set_init(&s);
    s.has_plot = 1;
    s.plot = 5;
    CHECK(auto_settings_apply(&s, why, sizeof why) == -1 && Auto.plot == 1);
    auto_settings_set_init(&s);
    s.has_range[2] = 1;
    s.range[2] = 3; /* ymin above ymax */
    CHECK(auto_settings_apply(&s, why, sizeof why) == -1 && Auto.ymin == -1);

    /* Mark values: a parameter of AUTO's or T, into AUTO's user points */
    auto_settings_set_init(&s);
    s.nmarks = 2;
    std::strcpy(s.mark_name[0], "phi");
    s.mark_value[0] = 0.5;
    std::strcpy(s.mark_name[1], "t");
    s.mark_value[1] = 20;
    CHECK(auto_settings_apply(&s, why, sizeof why) == 0 && Auto.nper == 2 && NAutoUzr == 2);
    CHECK(UzrPar[0] == 2 && outperiod[0] == 0.5 && UzrPar[1] == 10 && outperiod[1] == 20);
    std::strcpy(s.mark_name[1], "v"); /* a variable is no user point */
    CHECK(auto_settings_apply(&s, why, sizeof why) == -1 && UzrPar[1] == 10);

    TEST_REPORT("auto_settings");
}
