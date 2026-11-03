/* autox_stop: why autoX ended a branch, from what stplae/stplbv saw at its
   last point (docs/ui-v2.md T23). tools/servercheck.py runs autoX into each
   limit through the protocol; this checks the order the reasons are tried
   in and their words. */
#include "xpptest.h"
#include "session.h"
#include "model.h"
#include "autox_stop.h"
#include "autox_unknowns.h"

#include <cmath>
#include <cstring>

namespace {

/* a point inside every limit: nothing ends the branch but what a test sets */
xpp::AutoxStopAt inside()
{
    xpp::AutoxStopAt at{};
    at.br = 1;
    at.pt = 7;
    at.ipar = xpp::client_session().autox_state.parameter_layout.solver_index(0);
    at.par = 0.5;
    at.norm = 1;
    at.rl0 = 0, at.rl1 = 2, at.a0 = 0, at.a1 = 10;
    at.nmx = 100;
    return at;
}

} // namespace

int main()
{
    xpp::client_session().model().size_parameters(1);
    xpp::client_session().model().upar_names[0] = "iapp";
    xpp::client_session().model().nupar = 1;
    xpp::client_session().autox_state.parameter_layout.model_parameters=1;
    xpp::AutoxStop &stop = xpp::client_session().autox_state.stop;

    xpp::AutoxStopAt at = inside();
    at.par = 2.5;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_PAR_MAX);
    at.par = -1;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_PAR_MIN);
    at = inside();
    at.norm = 11;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_NORM_MAX);
    at.norm = -1; /* autoX's measure may be a variable's value */
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_NORM_MIN);
    at = inside();
    at.pt = 100;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_NPTS);
    at.par = 3; /* a limit is named before the point count */
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_PAR_MAX);
    at.mark = 1;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_MARK);
    at.noconv = 1;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_NOCONV); /* no NOTE said how */
    at.user = 1; /* the user's Stop first */
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_USER);

    /* the words, and the record the autoxinfo event sends */
    xpp::AutoxStopInfo st;
    xpp::autox_stop_clear(stop);
    xpp::autox_stop_last(stop, &st);
    CHECK(st.why == xpp::AUTOX_STOP_NONE && std::strcmp(st.text, "") == 0);
    at = inside();
    at.par = 2.25;
    xpp::autox_stop_branch_end(xpp::client_session(), &at);
    xpp::autox_stop_last(stop, &st);
    CHECK(st.why == xpp::AUTOX_STOP_PAR_MAX && st.br == 1 && st.pt == 7 && st.value == 2.25 && st.limit == 2);
    CHECK_STR(st.key, "parameter_max");
    CHECK_STR(st.text, "parameter iapp reached parameter_max (2)");
    at.ipar = xpp::autox_unknown::period;
    at.par = -0.5;
    xpp::autox_stop_branch_end(xpp::client_session(), &at);
    xpp::autox_stop_last(stop, &st);
    CHECK_STR(st.text, "the period T reached parameter_min (0)");

    /* a NOTE says how the solver failed; it counts for the next end only */
    at = inside();
    at.noconv = 1;
    at.br = -2; /* autoX's sign is stability, not part of the number */
    xpp::autox_stop_noconv(stop, {xpp::AUTOX_STOP_NOCONV_MIN, -1e-5, 0.0001, 5, 1e-7, 1e-6, 1e-31});
    xpp::autox_stop_branch_end(xpp::client_session(), &at);
    xpp::autox_stop_last(stop, &st);
    CHECK(st.why == xpp::AUTOX_STOP_NOCONV_MIN && st.br == 2 && st.value == 1e-5 && st.limit == 0.0001);
    CHECK_STR(st.key, "noconv-min");
    CHECK_STR(st.text, "no convergence even at the smallest step (min_step_size 0.0001; max_newton_iterations 5, parameter_tolerance 1e-07, state_tolerance 1e-06): "
                       "the linear system is singular (smallest pivot 1e-31 < 1e-30)");
    xpp::autox_stop_branch_end(xpp::client_session(), &at);
    xpp::autox_stop_last(stop, &st);
    CHECK(st.why == xpp::AUTOX_STOP_NOCONV && std::isnan(st.value));
    CHECK_STR(st.text, "no convergence");
    at = inside();
    at.nmx = 7;
    xpp::autox_stop_branch_end(xpp::client_session(), &at);
    xpp::autox_stop_last(stop, &st);
    CHECK_STR(st.text, "the branch reached max_steps (7)");

    at = inside();
    at.noconv = 1;
    xpp::autox_stop_noconv(stop, {xpp::AUTOX_STOP_NOCONV_SWITCH_FIXED, 0.0, 0.0});
    xpp::autox_stop_branch_end(xpp::client_session(), &at);
    xpp::autox_stop_last(stop, &st);
    CHECK_STR(st.key, "noconv-switch-fixed");
    xpp::autox_stop_clear(stop);
    xpp::autox_stop_last(stop, &st);
    CHECK(st.why == xpp::AUTOX_STOP_NONE);

    /* W518: a cycle branch's period growing without bound ends the branch by the end its equilibria name, before any limit */
    at = inside();
    at.global = 1;
    at.par = 3; /* past parameter_max: the global end is still the reason, and the user's Stop is first */
    stop.global.kind = xpp::AutoxPointKind::Snic;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_SNIC);
    stop.global.kind = xpp::AutoxPointKind::Homoclinic;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_HOMOCLINIC);
    stop.global.kind = xpp::AutoxPointKind::Heteroclinic;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_HETEROCLINIC);
    stop.global.kind = xpp::AutoxPointKind::End; /* no end fits */
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_PERIOD);
    at.user = 1;
    CHECK(xpp::autox_stop_why(stop, &at) == xpp::AUTOX_STOP_USER);

    TEST_REPORT("autox_stop");
}
