#!/usr/bin/env -S uv run --locked python
"""W252 (#308): on a tree made by ../rhs-ceiling/apply_patch.py and
../auto-parallel/apply_parallel.py (never the repository), XPP_JAC=1 with
XPP_HAND_RHS=4 gives the exact Jacobian of jac_ad.inc to

  AUTO        autevd.cpp sets JAC=1, so funi takes dfdu/dfdp from func instead of
              differencing the right-hand side; func fills them (and f) in one pass
  Stiff       stiff.cpp's jacobn fills dermat from it (the model is autonomous:
              df/dt = 0) instead of n + 2 right-hand-side calls

    uv run --locked python apply_jacobian.py TREE
"""
import shutil, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
core = Path(sys.argv[1]) / 'core'
shutil.copy(HERE / 'jac_ad.inc', core / 'jac_ad.inc')
NL = chr(10)


def patch(name, old, new):
    p = core / name
    t = p.read_text()
    assert t.count(old) == 1, (name, old)
    p.write_text(t.replace(old, new))


SWITCH = NL.join([
    '#include <algorithm>',
    '#include <cmath>',
    '#include <cstdio>',
    '#include <cstdlib>',
    'namespace {',
    'bool jac_on()',
    '{',
    '  static const bool on = [] { const char *e = std::getenv("XPP_JAC"); return e && e[0] == \'1\'; }();',
    '  return on;',
    '}',
    '} // namespace',
    ''])

patch('autevd.cpp', '  st.run.jac=0;',
      '  st.run.jac=std::getenv("XPP_JAC") && std::getenv("XPP_JAC")[0]==\'1\' && std::getenv("XPP_HAND_RHS") '
      '&& std::getenv("XPP_HAND_RHS")[0]==\'4\' ? 1 : 0; /* W252 */')
t = (core / 'autevd.cpp').read_text()
(core / 'autevd.cpp').write_text('#include <cstdlib>' + NL + t)

patch('autpp.cpp', '#include "hand_rhs.inc"', '#include "hand_rhs.inc"' + NL + '#include "jac_ad.inc"' + NL + SWITCH)
patch('autpp.cpp', '''     double fixed[21];
     hand_core<xpp::math::exp, xpp::math::pow>(u, f, fixed, par[iapp]);''', NL.join([
    '     if (jac_on() && ijac != 0) { /* W252: f, dfdu and df/dIapp in one pass */',
    '       double dfdiapp[8];',
    '       jac_ad(u, par[iapp], f, dfdu, dfdiapp);',
    '       static int checks = std::getenv("XPP_JAC_CHECK") ? 20 : 0; /* compare with central differences */',
    '       if (checks > 0) {',
    '         checks--;',
    '         double worst = 0, fp[8], fm[8], fx[21];',
    '         for (int c = 0; c <= 8; c++) {',
    '           double up[8], um[8], pp = par[iapp], pm = par[iapp];',
    '           for (int k = 0; k < 8; k++) up[k] = um[k] = u[k];',
    '           const double h = 1e-6 * (std::fabs(c < 8 ? u[c] : pp) + 1);',
    '           if (c < 8) { up[c] += h; um[c] -= h; } else { pp += h; pm -= h; }',
    '           hand_core<xpp::math::exp, xpp::math::pow>(up, fp, fx, pp);',
    '           hand_core<xpp::math::exp, xpp::math::pow>(um, fm, fx, pm);',
    '           for (int r = 0; r < 8; r++) {',
    '             const double fd = (fp[r] - fm[r]) / (2 * h), an = c < 8 ? dfdu[r + c * 8] : dfdiapp[r];',
    '             worst = std::max(worst, std::fabs(fd - an) / std::max(1.0, std::fabs(an)));',
    '           }',
    '         }',
    '         if (FILE *out = std::fopen(std::getenv("XPP_JAC_CHECK"), "a")) { /* XPP_JAC_CHECK names the file */',
    '           std::fprintf(out, "max |J_ad - J_cd| / max(1, |J_ad|) = %.3g\\n", worst);',
    '           std::fclose(out);',
    '         }',
    '       }',
    '       for (int k = 0; k < ndim; k++) dfdp[k + iapp * ndim] = dfdiapp[k];',
    '       return 0;',
    '     }',
    '     double fixed[21];',
    '     hand_core<xpp::math::exp, xpp::math::pow>(u, f, fixed, par[iapp]);']))

head = 'void jacobn(xpp::Session &s, double x, double *y, double *dfdx, double *dermat, double eps, double *work, int n)' + NL + '{' + NL
patch('stiff.cpp', head, '#include "hand_rhs.inc"' + NL + '#include "jac_ad.inc"' + NL + SWITCH + NL + head + NL.join([
    ' if (hand_mode() == 4 && jac_on()) { /* W252: the exact Jacobian; autonomous, so df/dt = 0 */',
    '   static const double *iapp = xpp::value_slot(s, "Iapp");',
    '   double f[8], dfdu[64], dfdiapp[8];',
    '   jac_ad(y, *iapp, f, dfdu, dfdiapp);',
    '   for (int r = 0; r < n; r++) {',
    '     dfdx[r] = 0;',
    '     for (int c = 0; c < n; c++) dermat[r * n + c] = dfdu[r + c * n];',
    '   }',
    '   return;',
    ' }',
    '']))
print('jacobian prototype in', core)
