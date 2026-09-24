/* auto_stop: why AUTO ended a branch, from what stplae/stplbv saw at its
   last point (docs/ui-v2.md T23). tools/servercheck.py runs AUTO into each
   limit through the protocol; this checks the order the reasons are tried
   in and their words. */
#include "xpptest.h"
#include "auto_stop.h"
#include "xpplim.h"

#include <cmath>
#include <cstring>

extern "C" {
extern char upar_names[][XPP_NAME_MAX + 1];
extern int AutoPar[8];
extern int NAutoPar;
extern int NUPAR;
}

namespace {

/* a point inside every limit: nothing ends the branch but what a test sets */
AutoStopAt inside()
{
    AutoStopAt at{};
    at.br = 1;
    at.pt = 7;
    at.ipar = 0;
    at.par = 0.5;
    at.norm = 1;
    at.rl0 = 0, at.rl1 = 2, at.a0 = 0, at.a1 = 10;
    at.nmx = 100;
    return at;
}

} // namespace

int main()
{
    std::strcpy(upar_names[0], "iapp");
    NUPAR = 1;
    NAutoPar = 1;
    AutoPar[0] = 0;

    AutoStopAt at = inside();
    at.par = 2.5;
    CHECK(auto_stop_why(&at) == AUTO_STOP_PAR_MAX);
    at.par = -1;
    CHECK(auto_stop_why(&at) == AUTO_STOP_PAR_MIN);
    at = inside();
    at.norm = 11;
    CHECK(auto_stop_why(&at) == AUTO_STOP_NORM_MAX);
    at.norm = -1; /* AUTO's measure may be a variable's value */
    CHECK(auto_stop_why(&at) == AUTO_STOP_NORM_MIN);
    at = inside();
    at.pt = 100;
    CHECK(auto_stop_why(&at) == AUTO_STOP_NPTS);
    at.par = 3; /* a limit is named before the point count */
    CHECK(auto_stop_why(&at) == AUTO_STOP_PAR_MAX);
    at.mark = 1;
    CHECK(auto_stop_why(&at) == AUTO_STOP_MARK);
    at.noconv = 1;
    CHECK(auto_stop_why(&at) == AUTO_STOP_NOCONV); /* no NOTE said how */
    at.user = 1; /* the user's Stop first */
    CHECK(auto_stop_why(&at) == AUTO_STOP_USER);

    /* the words, and the record the autoinfo event sends */
    AutoStopInfo st;
    auto_stop_clear();
    auto_stop_last(&st);
    CHECK(st.why == AUTO_STOP_NONE && std::strcmp(st.text, "") == 0);
    at = inside();
    at.par = 2.25;
    auto_stop_branch_end(&at);
    auto_stop_last(&st);
    CHECK(st.why == AUTO_STOP_PAR_MAX && st.br == 1 && st.pt == 7 && st.value == 2.25 && st.limit == 2);
    CHECK_STR(st.key, "parmax");
    CHECK_STR(st.text, "parameter iapp reached Par Max (2)");
    at.ipar = 10;
    at.par = -0.5;
    auto_stop_branch_end(&at);
    auto_stop_last(&st);
    CHECK_STR(st.text, "the period T reached Par Min (0)");

    /* a NOTE says how the solver failed; it counts for the next end only */
    at = inside();
    at.noconv = 1;
    at.br = -2; /* AUTO's sign is stability, not part of the number */
    auto_stop_noconv(AUTO_STOP_NOCONV_MIN, -1e-5, 0.0001);
    auto_stop_branch_end(&at);
    auto_stop_last(&st);
    CHECK(st.why == AUTO_STOP_NOCONV_MIN && st.br == 2 && st.value == 1e-5 && st.limit == 0.0001);
    CHECK_STR(st.key, "noconv-min");
    CHECK_STR(st.text, "no convergence even at the smallest step (Dsmin 0.0001)");
    auto_stop_branch_end(&at);
    auto_stop_last(&st);
    CHECK(st.why == AUTO_STOP_NOCONV && std::isnan(st.value));
    CHECK_STR(st.text, "no convergence");
    at = inside();
    at.nmx = 7;
    auto_stop_branch_end(&at);
    auto_stop_last(&st);
    CHECK_STR(st.text, "the branch reached Max points (NMX 7)");

    CHECK_STR(auto_stop_key(AUTO_STOP_NOCONV_SWITCH_FIXED), "noconv-switch-fixed");
    CHECK(auto_stop_key(AUTO_STOP_N) == nullptr);
    auto_stop_clear();
    auto_stop_last(&st);
    CHECK(st.why == AUTO_STOP_NONE);

    TEST_REPORT("auto_stop");
}
