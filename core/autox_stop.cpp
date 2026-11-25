#include "autox_unknowns.h"
/* Why autoX ended a branch (autox_stop.h, docs/ui-v2.md T23): the reason
   store_diagram_algebraic/store_diagram_boundary saw, in words, for autoX's Output and the autoxinfo event. */
#include <cmath>
#include <cstdlib>
#include <string>
#include <memory>
#include <new>
#include <utility>

#include "autox_nox.h"
#include "xpp_math_autox.h" /* autox_rsmall */
#include "autox_state.h"
#include "autox_stop.h"
#include "xpp_log.h"
#include "xpp_mem.h"
#include "autox_codes.h"
#include "xpp_error.h"
#include "xpp_text.h"

namespace xpp {

namespace {

const char *const keys[AUTOX_STOP_N] = {
    "", "limits.parameter_min", "limits.parameter_max", "limits.norm_min", "limits.norm_max", "limits.steps", "user", "mark",
    "noconv", "noconv-fixed", "noconv-min", "noconv-switch-fixed", "noconv-switch-min", "jacobian",
    "period_unbounded", "snic", "homoclinic", "heteroclinic", "hopf_end", "branch_closed",
};

/* the continuation parameter as the text names it */
std::string par_name(const AutoxParts &s, long ipar)
{
    if (ipar == xpp::autox_unknown::period) return "the period T";
    const int k = s.autox_state->parameter_layout.model_index(static_cast<int>(ipar));
    const char *name = autox_par_name(s, k);
    return name ? std::string("parameter ") + name : std::string("the parameter");
}

/* a limit as a person typed it: autoX keeps some as floats (0.0009999999) */
std::string num(double v) { return std::isfinite(v) ? xpp::format("{:g}", v) : std::string("?"); }

/* the Newton options that gave up, after the step's: "; ITNW 5, EPSL
   1e-07, EPSU 1e-07" (nothing for a solver without them) */
std::string newton(const AutoxNoConvergence &nc)
{
    if (nc.itnw <= 0) return "";
    return "; newton.max_iterations " + std::to_string(nc.itnw) + ", newton.parameter_tolerance " + num(nc.epsl) + ", newton.state_tolerance " + num(nc.epsu);
}

/* what a stopped orbit's no-convergence most likely needs: the mesh (W877), as the Numerics show it; nothing for an
   equilibrium branch, which has no mesh */
std::string mesh_hint(const AutoxParts &s, const AutoxStopAt &at)
{
    return at.boundary ? "; the orbit may need a larger mesh.intervals (now " + std::to_string(s.autox_state->options.ntst) + ")" : "";
}

/* the linear system the last Newton step solved was singular */
std::string singular(const AutoxNoConvergence &nc)
{
    if (!(nc.pivot < autox_rsmall)) return "";
    return ": the linear system is singular (smallest pivot " + num(nc.pivot) + " < " + num(autox_rsmall) + ")";
}

/* the reason in words, and what reached which limit */
std::string describe(const AutoxParts &s, const AutoxStop &st, int why, const AutoxStopAt &at, double &value, double &limit)
{
    value = limit = NAN;
    switch (why) {
    case AUTOX_STOP_PAR_MIN:
        value = at.par, limit = at.rl0;
        return par_name(s, at.ipar) + " reached limits.parameter_min (" + num(at.rl0) + ")";
    case AUTOX_STOP_PAR_MAX:
        value = at.par, limit = at.rl1;
        return par_name(s, at.ipar) + " reached limits.parameter_max (" + num(at.rl1) + ")";
    case AUTOX_STOP_NORM_MIN:
        value = at.norm, limit = at.a0;
        return "the norm reached limits.norm_min (" + num(at.a0) + ")";
    case AUTOX_STOP_NORM_MAX:
        value = at.norm, limit = at.a1;
        return "the norm reached limits.norm_max (" + num(at.a1) + ")";
    case AUTOX_STOP_NPTS:
        value = static_cast<double>(at.pt), limit = static_cast<double>(at.nmx);
        return "the branch reached limits.steps (" + std::to_string(at.nmx) + ")";
    case AUTOX_STOP_USER:
        return "by the user (Stop)";
    case AUTOX_STOP_MARK:
        value = at.par;
        return par_name(s, at.ipar) + " reached a Mark value set to stop (" + num(at.par) + ")";
    case AUTOX_STOP_NOCONV_FIXED:
        value = st.noconv.ds;
        return "no convergence with a fixed step size (step.adapt_every 0" + newton(st.noconv) + ")" + singular(st.noconv);
    case AUTOX_STOP_NOCONV_MIN:
        value = st.noconv.ds, limit = st.noconv.dsmin;
        return "no convergence even at the smallest step (step.min " + num(st.noconv.dsmin) + newton(st.noconv) + ")" +
               singular(st.noconv) + mesh_hint(s, at);
    case AUTOX_STOP_NOCONV_SWITCH_FIXED:
        value = st.noconv.ds;
        return "no convergence switching branches with a fixed step size (step.adapt_every 0" + newton(st.noconv) + ")" +
               singular(st.noconv);
    case AUTOX_STOP_NOCONV_SWITCH_MIN:
        value = st.noconv.ds, limit = st.noconv.dsmin;
        return "no convergence switching branches, even at the smallest step (step.min " + num(st.noconv.dsmin) +
               newton(st.noconv) + ")" + singular(st.noconv) + mesh_hint(s, at);
    case AUTOX_STOP_NOCONV:
        return "no convergence";
    case AUTOX_STOP_JACOBIAN:
        return "the exact Jacobian is not finite (see the error)";
    case AUTOX_STOP_PERIOD:
    case AUTOX_STOP_SNIC:
    case AUTOX_STOP_HOMOCLINIC:
    case AUTOX_STOP_HETEROCLINIC:
        value = st.global.period;
        return par_name(s, at.ipar) + " = " + num(at.par) + ": " + autox_global_text(s, st.global);
    case AUTOX_STOP_HOPF_END:
        value = st.hopf_end.amplitude;
        return par_name(s, at.ipar) + " = " + num(at.par) + ": " + autox_hopf_end_text(s, st.hopf_end);
    case AUTOX_STOP_CLOSED:
        value = at.par;
        return par_name(s, at.ipar) + " = " + num(at.par) + ": " + autox_closure_text(st.closure_gap.value_or(NAN), st.closure_epss);
    default:
        return "";
    }
}

/* the reason a cycle branch's period growing without bound gives, by the end named */
int global_why(AutoxPointKind kind)
{
    switch (kind) {
    case AutoxPointKind::Snic: return AUTOX_STOP_SNIC;
    case AutoxPointKind::Homoclinic: return AUTOX_STOP_HOMOCLINIC;
    case AutoxPointKind::Heteroclinic: return AUTOX_STOP_HETEROCLINIC;
    default: return AUTOX_STOP_PERIOD;
    }
}

} // namespace

void autox_stop_noconv(AutoxStop &st, const AutoxNoConvergence &noconv)
{
    st.noconv = noconv;
    if (noconv.why < AUTOX_STOP_NOCONV || noconv.why >= AUTOX_STOP_N) st.noconv.why = AUTOX_STOP_NOCONV;
    st.noconv.ds = std::fabs(noconv.ds);
}

int autox_stop_why(const AutoxStop &st, const AutoxStopAt *at)
{
    if (at->user) return AUTOX_STOP_USER;
    if (at->global) return global_why(st.global.kind);
    if (at->hopf_end) return AUTOX_STOP_HOPF_END;
    if (at->closed) return AUTOX_STOP_CLOSED;
    if (at->noconv) return st.noconv.why;
    if (at->mark) return AUTOX_STOP_MARK;
    if (at->par < at->rl0) return AUTOX_STOP_PAR_MIN;
    if (at->par > at->rl1) return AUTOX_STOP_PAR_MAX;
    if (at->norm < at->a0) return AUTOX_STOP_NORM_MIN;
    if (at->norm > at->a1) return AUTOX_STOP_NORM_MAX;
    if (at->pt >= at->nmx) return AUTOX_STOP_NPTS;
    return AUTOX_STOP_USER; /* byeautox's iflag without a cancel: an X11-style abort */
}

void autox_stop_clear(AutoxStop &st)
{
    st.last_why = AUTOX_STOP_NONE;
    st.last_text.clear();
    st.ends.clear();
    st.followed = 0;
    st.noconv = {};
    st.notes.clear();
    st.start_refined = false;
    st.period_watch = {};
    st.global = {};
    st.amplitude_watch = {};
    st.hopf_end = {};
    st.closure_gap.reset();
    st.hopf_failure_noted = false;
}

void autox_stop_note(AutoxRun &run, std::string text)
{
    if (xpp::log_autox_enabled()) xpp::log_autox("Warning: {}\n", text);
    else xpp::log(XPP_LOG_WARN, "autoX: {}\n", text);
    try {
        run.stop.notes.push_back(std::move(text));
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording an autoX note");
    }
}

void autox_stop_merge(AutoxStop &shown, AutoxStop &&run)
{
    if (run.last_why != AUTOX_STOP_NONE) {
        shown.last_why = run.last_why;
        shown.last_br = run.last_br;
        shown.last_pt = run.last_pt;
        shown.last_value = run.last_value;
        shown.last_limit = run.last_limit;
        shown.last_text = std::move(run.last_text);
    }
    for (AutoxBranchEnd &end : run.ends) shown.ends.push_back(std::move(end));
    for (std::string &note : run.notes) shown.notes.push_back(std::move(note));
    shown.start_refined |= run.start_refined;
}

void autox_stop_last(const AutoxStop &st, AutoxStopInfo *out)
{
    out->why = st.last_why;
    out->key = keys[st.last_why];
    out->text = st.last_text.c_str();
    out->br = st.last_br;
    out->pt = st.last_pt;
    out->value = st.last_value;
    out->limit = st.last_limit;
}

void autox_stop_branch_end(AutoxRun &run, const AutoxStopAt *at)
{
    AutoxStop &st = run.stop;
    try {
        const int why = run.jacobian_failure ? AUTOX_STOP_JACOBIAN : autox_stop_why(st, at);
        double value, limit;
        std::string text = describe(*run.parts, st, why, *at, value, limit);
        st.last_why = why;
        st.last_br = std::labs(at->br);
        st.last_pt = std::labs(at->pt);
        st.last_value = value;
        st.last_limit = limit;
        st.last_text = std::move(text);
        st.ends.push_back({why, keys[why], st.last_br, st.last_pt, st.last_text});
        /* no convergence is no limit the user set: said where autoX's Output is not shown too */
        if (why >= AUTOX_STOP_NOCONV && why <= AUTOX_STOP_NOCONV_SWITCH_MIN && !xpp::log_autox_enabled())
            xpp::log(XPP_LOG_WARN, "autoX: branch {:d} stopped at point {:d}: {}\n", st.last_br, st.last_pt, st.last_text);
    } catch (...) {
        st.last_why = AUTOX_STOP_NONE;
    }
    st.noconv = {}; /* a NOTE says how the next one failed */
}


} // namespace xpp
