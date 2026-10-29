#!/usr/bin/env -S uv run --locked python
"""W249 (#305): patch a copy of the pinned xppautX tree (never the repository) so
my_rhs dispatches to hand_rhs.inc when XPP_HAND_RHS is 1 or 2, and with --tcc
(W250, #306) to tcc_rhs.inc's TinyCC-compiled model when it is 3 (link with -ltcc).

    uv run --locked python apply_patch.py TREE [--tcc]   # TREE: a copy of the paper build's modern/ folder
"""
import shutil, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
tree = Path(sys.argv[1])
tcc = '--tcc' in sys.argv[2:]
src = tree / 'core' / 'my_rhs.cpp'
t = src.read_text()
head = 'int my_rhs(xpp::Session &s, double t, double *y, double *ydot, int neq)\n{\n'
assert t.count(head) == 1 and 'hand_rhs.inc' not in t
t = t.replace(head, '#include "hand_rhs.inc"\n' + ('#include "tcc_rhs.inc"\n' if tcc else '') + '\n' + head +
              ('  if (hand_mode() == 3) { tcc_rhs(s, t, y, ydot); return 1; }\n' if tcc else '') +
              '  if (hand_mode() == 1) { hand_rhs<xpp::math::exp, xpp::math::pow>(s, t, y, ydot); return 1; }\n'
              '  if (hand_mode() == 2) { hand_rhs<c_exp, c_pow>(s, t, y, ydot); return 1; }\n')
src.write_text(t)
for inc in ['hand_rhs.inc'] + (['tcc_rhs.inc'] if tcc else []):
    shutil.copy(HERE / inc, tree / 'core' / inc)
print('patched', src)
