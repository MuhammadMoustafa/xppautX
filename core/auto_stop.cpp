#include "auto_unknowns.h"
/* Why AUTO ended a branch (auto_stop.h, docs/ui-v2.md T23): the reason
   stplae/stplbv saw, in words, for AUTO's Output and the autoinfo event. */
#include <cmath>
#include <cstdlib>
#include <string>

#include "auto_nox.h"
#include "xpp_math_auto.h" /* auto_rsmall */
#include "session.h"
#include "auto_stop.h"
#include "xpp_log.h"
#include "xpp_mem.h"

namespace xpp {

namespace {

const char *const keys[AUTO_STOP_N] = {
    "", "parameter_min", "parameter_max", "norm_min", "norm_max", "max_steps", "user", "mark",
    "noconv", "noconv-fixed", "noconv-min", "noconv-switch-fixed", "noconv-switch-min", "jacobian",
    "period_unbounded", "snic", "homoclinic", "heteroclinic",
};

/* the continuation parameter as the text names it */
std::string par_name(const xpp::Session &s, long ipar)
{
    if (ipar == xpp::auto_unknown::period) return "the period T";
    const int k = s.auto_state.parameter_layout.model_index(static_cast<int>(ipar));
    const char *name = auto_par_name(s, k);
    return name ? std::string("parameter ") + name : std::string("the parameter");
}

/* a limit as a person typed it: AUTO keeps some as floats (0.0009999999) */
std::string num(double v) { return std::isfinite(v) ? xpp::format("{:g}", v) : std::string("?"); }

/* the Newton settings that gave up, after the step's: "; ITNW 5, EPSL
   1e-07, EPSU 1e-07" (nothing for a solver without them) */
std::string newton(const AutoNoConvergence &nc)
{
    if (nc.itnw <= 0) return "";
    return "; max_newton_iterations " + std::to_string(nc.itnw) + ", parameter_tolerance " + num(nc.epsl) + ", state_tolerance " + num(nc.epsu);
}

/* the linear system the last Newton step solved was singular */
std::string singular(const AutoNoConvergence &nc)
{
    if (!(nc.pivot < auto_rsmall)) return "";
    return ": the linear system is singular (smallest pivot " + num(nc.pivot) + " < " + num(auto_rsmall) + ")";
}

/* the reason in words, and what reached which limit */
std::string describe(const xpp::Session &s, const AutoStop &st, int why, const AutoStopAt &at, double &value, double &limit)
{
    value = limit = NAN;
    switch (why) {
    case AUTO_STOP_PAR_MIN:
        value = at.par, limit = at.rl0;
        return par_name(s, at.ipar) + " reached parameter_min (" + num(at.rl0) + ")";
    case AUTO_STOP_PAR_MAX:
        value = at.par, limit = at.rl1;
        return par_name(s, at.ipar) + " reached parameter_max (" + num(at.rl1) + ")";
    case AUTO_STOP_NORM_MIN:
        value = at.norm, limit = at.a0;
        return "the norm reached norm_min (" + num(at.a0) + ")";
    case AUTO_STOP_NORM_MAX:
        value = at.norm, limit = at.a1;
        return "the norm reached norm_max (" + num(at.a1) + ")";
    case AUTO_STOP_NPTS:
        value = static_cast<double>(at.pt), limit = static_cast<double>(at.nmx);
        return "the branch reached max_steps (" + std::to_string(at.nmx) + ")";
    case AUTO_STOP_USER:
        return "by the user (Stop)";
    case AUTO_STOP_MARK:
        value = at.par;
        return par_name(s, at.ipar) + " reached a Mark value set to stop (" + num(at.par) + ")";
    case AUTO_STOP_NOCONV_FIXED:
        value = st.noconv.ds;
        return "no convergence with a fixed step size (step_adapt_every 0" + newton(st.noconv) + ")" + singular(st.noconv);
    case AUTO_STOP_NOCONV_MIN:
        value = st.noconv.ds, limit = st.noconv.dsmin;
        return "no convergence even at the smallest step (min_step_size " + num(st.noconv.dsmin) + newton(st.noconv) + ")" +
               singular(st.noconv);
    case AUTO_STOP_NOCONV_SWITCH_FIXED:
        value = st.noconv.ds;
        return "no convergence switching branches with a fixed step size (step_adapt_every 0" + newton(st.noconv) + ")" +
               singular(st.noconv);
    case AUTO_STOP_NOCONV_SWITCH_MIN:
        value = st.noconv.ds, limit = st.noconv.dsmin;
        return "no convergence switching branches, even at the smallest step (min_step_size " + num(st.noconv.dsmin) +
               newton(st.noconv) + ")" + singular(st.noconv);
    case AUTO_STOP_NOCONV:
        return "no convergence";
    case AUTO_STOP_JACOBIAN:
        return "the exact Jacobian is not finite (see the error)";
    case AUTO_STOP_PERIOD:
    case AUTO_STOP_SNIC:
    case AUTO_STOP_HOMOCLINIC:
    case AUTO_STOP_HETEROCLINIC:
        value = st.global.period;
        return par_name(s, at.ipar) + " = " + num(at.par) + ": " + auto_global_text(s, st.global);
    default:
        return "";
    }
}

/* the reason a cycle branch's period growing without bound gives, by the end named */
int global_why(AutoPointKind kind)
{
    switch (kind) {
    case AutoPointKind::Snic: return AUTO_STOP_SNIC;
    case AutoPointKind::Homoclinic: return AUTO_STOP_HOMOCLINIC;
    case AutoPointKind::Heteroclinic: return AUTO_STOP_HETEROCLINIC;
    default: return AUTO_STOP_PERIOD;
    }
}

} // namespace

void auto_stop_noconv(AutoStop &st, const AutoNoConvergence &noconv)
{
    st.noconv = noconv;
    if (noconv.why < AUTO_STOP_NOCONV || noconv.why >= AUTO_STOP_N) st.noconv.why = AUTO_STOP_NOCONV;
    st.noconv.ds = std::fabs(noconv.ds);
}

int auto_stop_why(const AutoStop &st, const AutoStopAt *at)
{
    if (at->user) return AUTO_STOP_USER;
    if (at->global) return global_why(st.global.kind);
    if (at->noconv) return st.noconv.why;
    if (at->mark) return AUTO_STOP_MARK;
    if (at->par < at->rl0) return AUTO_STOP_PAR_MIN;
    if (at->par > at->rl1) return AUTO_STOP_PAR_MAX;
    if (at->norm < at->a0) return AUTO_STOP_NORM_MIN;
    if (at->norm > at->a1) return AUTO_STOP_NORM_MAX;
    if (at->pt >= at->nmx) return AUTO_STOP_NPTS;
    return AUTO_STOP_USER; /* byeauto's iflag without a cancel: an X11-style abort */
}

void auto_stop_clear(AutoStop &st)
{
    st.last_why = AUTO_STOP_NONE;
    st.last_text.clear();
    st.noconv = {};
    st.notes.clear();
    st.start_refined = false;
    st.period_watch = {};
    st.global = {};
}

void auto_stop_note(Session &s, std::string text)
{
    if (xpp::log_auto_enabled()) xpp::log_auto("Warning: {}\n", text);
    else xpp::log(XPP_LOG_WARN, "AUTO: {}\n", text);
    try {
        s.auto_state.stop.notes.push_back(std::move(text));
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording an AUTO note");
    }
}

void auto_stop_last(const AutoStop &st, AutoStopInfo *out)
{
    out->why = st.last_why;
    out->key = keys[st.last_why];
    out->text = st.last_text.c_str();
    out->br = st.last_br;
    out->pt = st.last_pt;
    out->value = st.last_value;
    out->limit = st.last_limit;
}

void auto_stop_branch_end(Session &s, const AutoStopAt *at)
{
    AutoStop &st = s.auto_state.stop;
    try {
        const int why = s.auto_lib.jacobian_failure ? AUTO_STOP_JACOBIAN : auto_stop_why(st, at);
        double value, limit;
        std::string text = describe(s, st, why, *at, value, limit);
        st.last_why = why;
        st.last_br = std::labs(at->br);
        st.last_pt = std::labs(at->pt);
        st.last_value = value;
        st.last_limit = limit;
        st.last_text = std::move(text);
        xpp::log_auto("Branch {:d} stopped at point {:d}: {}\n", st.last_br, st.last_pt, st.last_text.c_str());
        /* no convergence is no limit the user set: said where AUTO's Output is not shown too */
        if (why >= AUTO_STOP_NOCONV && why <= AUTO_STOP_NOCONV_SWITCH_MIN && !xpp::log_auto_enabled())
            xpp::log(XPP_LOG_WARN, "AUTO: branch {:d} stopped at point {:d}: {}\n", st.last_br, st.last_pt, st.last_text);
    } catch (...) {
        st.last_why = AUTO_STOP_NONE;
    }
    st.noconv = {}; /* a NOTE says how the next one failed */
}


} // namespace xpp
