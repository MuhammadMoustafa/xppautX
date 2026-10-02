#!/bin/sh
# The core never calls the C library's transcendental functions directly
# (W159, issue #211): glibc's exp, log, pow, sin, cos, tan, atan, asin, acos
# and atan2 are dispatched at run time to FMA or SSE2 variants that round
# differently, its lgamma, erf and hyperbolic functions call them, and
# UCRT's and macOS's are other algorithms again, so one model gave different
# numbers on different machines. Core code calls xpp::math::exp, log, pow,
# sin, ... (core/xpp_math.h: CORE-MATH's, correctly rounded, the same bits
# everywhere). sqrt, fabs, floor, fmod and the like are exact in IEEE and
# stay the C library's. Comments are stripped first, a function named by
# address (std::sin, as a table entry) counts like a call, and the tests
# (which compare against the C library with a tolerance) are not checked.
# tools/sourcecheck.sh runs this. Usage: tools/mathcheck.sh
cd "$(dirname "$0")/.." || exit 1

# the one file that defines them
EXCLUDE="core/xpp_math.h"

NAMES='sin|cos|tan|asin|acos|atan|atan2|sinh|cosh|tanh|asinh|acosh|atanh|exp|exp2|expm1|log|log2|log10|log1p|pow|hypot|cbrt|erf|erfc|lgamma|tgamma|sincos'
# a call, not a member (x.exp(), p->exp()), a longer name (xpp_exp()), a
# string ("exp(O)rt") or a qualified name of ours (xpp::log, xpp::math::exp)
CALL="(^|[^a-zA-Z0-9_.>:\"])(std::|::)?($NAMES)[ 	]*\("
# a function named by address: std::sin, ::exp, &std::pow
ADDR="(^|[^a-zA-Z0-9_])(std::|::|&)($NAMES)[ 	]*[,;)}]"

bad=0
for f in core/*.cpp core/*.h; do
  [ -f "$f" ] || continue
  case " $EXCLUDE " in *" $f "*) continue ;; esac
  # (xpp::log called as log(XPP_LOG_..., inside namespace xpp, is the logger)
  hits=$(awk -f tools/strip_comments.awk "$f" | grep -nE "$CALL|$ADDR" | grep -vE 'log[ 	]*\((XPP_LOG_|XppLogLevel )')
  [ -n "$hits" ] || continue
  printf '%s\n' "$hits" | while IFS=: read -r lineno _; do
    printf '%s:%s: %s\n' "$f" "$lineno" "$(sed -n "${lineno}p" "$f")"
  done
  bad=1
done
if [ $bad -ne 0 ]; then
  echo "mathcheck: the C library's transcendental function called directly; use xpp::math::<name> (core/xpp_math.h)"
  exit 1
fi
echo "mathcheck ok: every core exp, log, pow, sin, cos, ... is xpp::math's"
