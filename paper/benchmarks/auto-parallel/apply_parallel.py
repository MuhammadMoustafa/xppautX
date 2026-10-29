#!/usr/bin/env -S uv run --locked python
"""W251 (#307): turn a tree patched by ../rhs-ceiling/apply_patch.py (never the
repository) into the parallel collocation prototype:

  core/setubv2.cpp, core/conpar2.cpp  this folder's copies (88e09741's, with the
                                      threads and XPP_FA_FIX; auto_par.inc)
  core/autpp.cpp                      func: XPP_HAND_RHS=4 calls hand_rhs.inc's pure
                                      core with Iapp from AUTO's par, writing no Session
                                      state (what makes the threads safe)
  core/my_rhs.cpp                     mode 4 integrates like mode 1
  --tcc                               also XPP_HAND_RHS=5: func calls tcc_rhs.inc's
                                      TinyCC-compiled model the same way (the tree
                                      patched with apply_patch.py --tcc, linked -ltcc)

    uv run --locked python apply_parallel.py TREE [--tcc]
"""
import shutil, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
core = Path(sys.argv[1]) / 'core'
tcc = '--tcc' in sys.argv[2:]
for name in ('setubv2.cpp', 'conpar2.cpp', 'auto_par.inc'):
    shutil.copy(HERE / name, core / name)
shutil.copy(HERE.parent / 'rhs-ceiling' / 'hand_rhs.inc', core / 'hand_rhs.inc')


def patch(name, old, new):
    p = core / name
    t = p.read_text()
    assert t.count(old) == 1, (name, old)
    p.write_text(t.replace(old, new))


patch('my_rhs.cpp', '  if (hand_mode() == 1) {', '  if (hand_mode() == 1 || hand_mode() == 4) {')
head = ('int func(xpp::Session &s, integer ndim, const double *u, const integer *icp, const double *par, '
        'integer ijac, double *f, double *dfdu, double *dfdp)\n{\n')
patch('autpp.cpp', head, '#include "hand_rhs.inc"\n\n' + head + '''   if (hand_mode() == 4) { /* W251: the pure compiled right-hand side */
     static const int iapp = [&] {
       const double *slot = xpp::value_slot(s, "Iapp");
       for (int k = 0; k < s.auto_state.npar; k++)
         if (&s.parser.constants[s.auto_state.par_index[k]] == slot) return k;
       std::abort();
     }();
     double fixed[21];
     hand_core<xpp::math::exp, xpp::math::pow>(u, f, fixed, par[iapp]);
     return 0;
   }
''')
if tcc:
    patch('my_rhs.cpp', '  if (hand_mode() == 3) {', '  if (hand_mode() == 3 || hand_mode() == 5) {')
    patch('autpp.cpp', '   if (hand_mode() == 4) {', '''   if (hand_mode() == 5) { /* W251: the pure TinyCC-compiled right-hand side */
     static const model_fn fn = tcc_compile();
     static const int iapp = [&] {
       const double *slot = xpp::value_slot(s, "Iapp");
       for (int k = 0; k < s.auto_state.npar; k++)
         if (&s.parser.constants[s.auto_state.par_index[k]] == slot) return k;
       std::abort();
     }();
     double fixed[21];
     fn(0.0, u, f, fixed, par[iapp]);
     return 0;
   }
   if (hand_mode() == 4) {''')
    patch('autpp.cpp', '#include "hand_rhs.inc"', '#include "hand_rhs.inc"' + chr(10) + '#include "tcc_rhs.inc"')
print('parallel prototype in', core)
