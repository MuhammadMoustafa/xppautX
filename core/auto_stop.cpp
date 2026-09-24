/* Why AUTO ended a branch (auto_stop.h, docs/ui-v2.md T23): the reason
   stplae/stplbv saw, in words, for AUTO's Output and the autoinfo event. */
#include <cmath>
#include <cstdlib>
#include <string>

#include "auto_stop.h"
#include "pop_list.h"
#include "xpp_io.h"
#include "xpp_log.h"

extern "C" {
extern int NAutoPar;
extern int AutoPar[8];
extern int NUPAR;
}

namespace {

constexpr long PERIOD = 10; /* AUTO's index of the period, T (auto_par_to_name) */

const char *const keys[AUTO_STOP_N] = {
    "", "parmin", "parmax", "normmin", "normmax", "npts", "user", "mark",
    "noconv", "noconv-fixed", "noconv-min", "noconv-switch-fixed", "noconv-switch-min",
};

/* the last "No convergence" NOTE */
int noconv_why = AUTO_STOP_NOCONV;
double noconv_ds = NAN, noconv_dsmin = NAN;

/* the last branch end */
int last_why = AUTO_STOP_NONE;
long last_br, last_pt;
double last_value = NAN, last_limit = NAN;
std::string last_text;

/* the continuation parameter as the text names it */
std::string par_name(long ipar)
{
    if (ipar == PERIOD) return "the period T";
    if (ipar >= 0 && ipar < NAutoPar && AutoPar[ipar] >= 0 && AutoPar[ipar] < NUPAR)
        return std::string("parameter ") + upar_names[AutoPar[ipar]];
    return "the parameter";
}

/* a limit as a person typed it: AUTO keeps some as floats (0.0009999999) */
std::string num(double v) { return std::isfinite(v) ? xpp::format("{:g}", v) : std::string("?"); }

/* the reason in words, and what reached which limit */
std::string describe(int why, const AutoStopAt &at, double &value, double &limit)
{
    value = limit = NAN;
    switch (why) {
    case AUTO_STOP_PAR_MIN:
        value = at.par, limit = at.rl0;
        return par_name(at.ipar) + " reached Par Min (" + num(at.rl0) + ")";
    case AUTO_STOP_PAR_MAX:
        value = at.par, limit = at.rl1;
        return par_name(at.ipar) + " reached Par Max (" + num(at.rl1) + ")";
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
        return par_name(at.ipar) + " reached a Mark value set to stop (" + num(at.par) + ")";
    case AUTO_STOP_NOCONV_FIXED:
        value = noconv_ds;
        return "no convergence with a fixed step size (IADS 0)";
    case AUTO_STOP_NOCONV_MIN:
        value = noconv_ds, limit = noconv_dsmin;
        return "no convergence even at the smallest step (Dsmin " + num(noconv_dsmin) + ")";
    case AUTO_STOP_NOCONV_SWITCH_FIXED:
        value = noconv_ds;
        return "no convergence switching branches with a fixed step size (IADS 0)";
    case AUTO_STOP_NOCONV_SWITCH_MIN:
        value = noconv_ds, limit = noconv_dsmin;
        return "no convergence switching branches, even at the smallest step (Dsmin " + num(noconv_dsmin) + ")";
    case AUTO_STOP_NOCONV:
        return "no convergence";
    default:
        return "";
    }
}

} // namespace

extern "C" {

void auto_stop_noconv(int why, double ds, double dsmin)
{
    noconv_why = why >= AUTO_STOP_NOCONV && why < AUTO_STOP_N ? why : AUTO_STOP_NOCONV;
    noconv_ds = std::fabs(ds);
    noconv_dsmin = dsmin;
}

int auto_stop_why(const AutoStopAt *at)
{
    if (at->user) return AUTO_STOP_USER;
    if (at->noconv) return noconv_why;
    if (at->mark) return AUTO_STOP_MARK;
    if (at->par < at->rl0) return AUTO_STOP_PAR_MIN;
    if (at->par > at->rl1) return AUTO_STOP_PAR_MAX;
    if (at->norm < at->a0) return AUTO_STOP_NORM_MIN;
    if (at->norm > at->a1) return AUTO_STOP_NORM_MAX;
    if (at->pt >= at->nmx) return AUTO_STOP_NPTS;
    return AUTO_STOP_USER; /* byeauto's iflag without a cancel: an X11-style abort */
}

void auto_stop_branch_end(const AutoStopAt *at)
{
    try {
        const int why = auto_stop_why(at);
        double value, limit;
        std::string text = describe(why, *at, value, limit);
        last_why = why;
        last_br = std::labs(at->br);
        last_pt = std::labs(at->pt);
        last_value = value;
        last_limit = limit;
        last_text = std::move(text);
        xpp_log_auto("Branch %ld stopped at point %ld: %s\n", last_br, last_pt, last_text.c_str());
    } catch (...) {
        last_why = AUTO_STOP_NONE;
    }
    noconv_why = AUTO_STOP_NOCONV; /* a NOTE says how the next one failed */
}

void auto_stop_clear(void)
{
    last_why = AUTO_STOP_NONE;
    last_text.clear();
    noconv_why = AUTO_STOP_NOCONV;
}

void auto_stop_last(AutoStopInfo *out)
{
    out->why = last_why;
    out->key = keys[last_why];
    out->text = last_text.c_str();
    out->br = last_br;
    out->pt = last_pt;
    out->value = last_value;
    out->limit = last_limit;
}

const char *auto_stop_key(int why) { return why >= 0 && why < AUTO_STOP_N ? keys[why] : nullptr; }

} // extern "C"
