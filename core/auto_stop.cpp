/* Why AUTO ended a branch (auto_stop.h, docs/ui-v2.md T23): the reason
   stplae/stplbv saw, in words, for AUTO's Output and the autoinfo event. */
#include <cmath>
#include <cstdlib>
#include <string>

#include "auto_nox.h"
#include "session.h"
#include "auto_stop.h"
#include "xpp_log.h"

namespace xpp {

namespace {

const char *const keys[AUTO_STOP_N] = {
    "", "parmin", "parmax", "normmin", "normmax", "npts", "user", "mark",
    "noconv", "noconv-fixed", "noconv-min", "noconv-switch-fixed", "noconv-switch-min",
};

/* the continuation parameter as the text names it */
std::string par_name(const xpp::Session &s, long ipar)
{
    if (ipar == AUTO_PERIOD_INDEX) return "the period T";
    const char *name = ipar >= 0 && ipar < s.auto_state.npar ? auto_par_name(s, static_cast<int>(ipar)) : nullptr;
    return name ? std::string("parameter ") + name : std::string("the parameter");
}

/* a limit as a person typed it: AUTO keeps some as floats (0.0009999999) */
std::string num(double v) { return std::isfinite(v) ? xpp::format("{:g}", v) : std::string("?"); }

/* the reason in words, and what reached which limit */
std::string describe(const xpp::Session &s, const AutoStop &st, int why, const AutoStopAt &at, double &value, double &limit)
{
    value = limit = NAN;
    switch (why) {
    case AUTO_STOP_PAR_MIN:
        value = at.par, limit = at.rl0;
        return par_name(s, at.ipar) + " reached Par Min (" + num(at.rl0) + ")";
    case AUTO_STOP_PAR_MAX:
        value = at.par, limit = at.rl1;
        return par_name(s, at.ipar) + " reached Par Max (" + num(at.rl1) + ")";
    case AUTO_STOP_NORM_MIN:
        value = at.norm, limit = at.a0;
        return "the norm reached Norm Min (" + num(at.a0) + ")";
    case AUTO_STOP_NORM_MAX:
        value = at.norm, limit = at.a1;
        return "the norm reached Norm Max (" + num(at.a1) + ")";
    case AUTO_STOP_NPTS:
        value = static_cast<double>(at.pt), limit = static_cast<double>(at.nmx);
        return "the branch reached Max points (NMX " + std::to_string(at.nmx) + ")";
    case AUTO_STOP_USER:
        return "by the user (Stop)";
    case AUTO_STOP_MARK:
        value = at.par;
        return par_name(s, at.ipar) + " reached a Mark value set to stop (" + num(at.par) + ")";
    case AUTO_STOP_NOCONV_FIXED:
        value = st.noconv_ds;
        return "no convergence with a fixed step size (IADS 0)";
    case AUTO_STOP_NOCONV_MIN:
        value = st.noconv_ds, limit = st.noconv_dsmin;
        return "no convergence even at the smallest step (Dsmin " + num(st.noconv_dsmin) + ")";
    case AUTO_STOP_NOCONV_SWITCH_FIXED:
        value = st.noconv_ds;
        return "no convergence switching branches with a fixed step size (IADS 0)";
    case AUTO_STOP_NOCONV_SWITCH_MIN:
        value = st.noconv_ds, limit = st.noconv_dsmin;
        return "no convergence switching branches, even at the smallest step (Dsmin " + num(st.noconv_dsmin) + ")";
    case AUTO_STOP_NOCONV:
        return "no convergence";
    default:
        return "";
    }
}

} // namespace

void auto_stop_noconv(AutoStop &st, int why, double ds, double dsmin)
{
    st.noconv_why = why >= AUTO_STOP_NOCONV && why < AUTO_STOP_N ? why : AUTO_STOP_NOCONV;
    st.noconv_ds = std::fabs(ds);
    st.noconv_dsmin = dsmin;
}

int auto_stop_why(const AutoStop &st, const AutoStopAt *at)
{
    if (at->user) return AUTO_STOP_USER;
    if (at->noconv) return st.noconv_why;
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
    st.noconv_why = AUTO_STOP_NOCONV;
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

const char *auto_stop_key(int why) { return why >= 0 && why < AUTO_STOP_N ? keys[why] : nullptr; }

void auto_stop_branch_end(Session &s, const AutoStopAt *at)
{
    AutoStop &st = s.auto_state.stop;
    try {
        const int why = auto_stop_why(st, at);
        double value, limit;
        std::string text = describe(s, st, why, *at, value, limit);
        st.last_why = why;
        st.last_br = std::labs(at->br);
        st.last_pt = std::labs(at->pt);
        st.last_value = value;
        st.last_limit = limit;
        st.last_text = std::move(text);
        xpp::log_auto("Branch {:d} stopped at point {:d}: {}\n", st.last_br, st.last_pt, st.last_text.c_str());
    } catch (...) {
        st.last_why = AUTO_STOP_NONE;
    }
    st.noconv_why = AUTO_STOP_NOCONV; /* a NOTE says how the next one failed */
}


} // namespace xpp
