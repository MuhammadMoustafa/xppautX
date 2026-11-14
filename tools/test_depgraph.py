#!/usr/bin/env python3
"""Self-test of tools/depgraph.py (W725, #803; the levels, W728, #806) on a tiny tree: a header
cycle, a module cycle, reach, a skipped `#if 0` include, an unplaced file, the levels (a
downward include passes, an upward one and one across an independent pair fail) and the
allowlist (its count exact: above, below or none left each fail)."""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depgraph  # noqa: E402

MODULES = {'low': ('the owner', ['a', 'a_*']), 'high': ('the user', ['b*', 'c*'])}
LEVELS = [('low', 'kernel', 'the owner sits lower'), ('high', 'domain model', 'the user sits higher')]


def tree(files):
    root = tempfile.mkdtemp()
    for path, text in files.items():
        os.makedirs(os.path.dirname(os.path.join(root, path)), exist_ok=True)
        with open(os.path.join(root, path), 'w') as f:
            f.write(text)
    return root


FILES = {
    'core/a.h': '#pragma once\n#include "a_x.h"\n',
    'core/a_x.h': '#pragma once\n#include "a.h"\n#include "b.h"\n',   # a.h <-> a_x.h; low -> high
    'core/b.h': '#pragma once\n// #include "c.h"\n#if 0\n#include "c.h"\n#endif\n',
    'core/c.h': '#pragma once\n',
    'core/b.cpp': '#include "b.h"\n#include "a.h"\n',   # high -> low
    'core/c.cpp': '#include "c.h"\n#include "a.h"\n',
    'tests/t.cpp': '#include "b.h"\n',
}


class Depgraph(unittest.TestCase):
    def setUp(self):
        self.r = depgraph.analyze(tree(FILES), MODULES)

    def test_header_cycle_found(self):
        cycles = self.r['header_cycles']
        self.assertEqual([c['files'] for c in cycles], [['core/a.h', 'core/a_x.h']])
        self.assertEqual(cycles[0]['lines'], ['core/a.h:2', 'core/a_x.h:2'])

    def test_module_cycle_found(self):
        self.assertEqual(self.r['module_cycles'], [['high', 'low']])
        pair = self.r['module_pairs'][0]
        self.assertEqual((pair['a'], pair['b']), ('high', 'low'))
        self.assertEqual(pair['ab'], ['core/b.cpp:2', 'core/c.cpp:2'])
        self.assertEqual(pair['ba'], ['core/a_x.h:3'])

    def test_reach_counts_cpps_through_headers(self):
        reach = self.r['reach']
        self.assertEqual(reach['core/a.h'], 2)    # b.cpp and c.cpp directly
        self.assertEqual(reach['core/b.h'], 2)    # c.cpp through a.h, a_x.h
        self.assertEqual(reach['core/c.h'], 1)    # the `#if 0` and commented includes do not count
        self.assertEqual(self.r['cpps'], 2)

    def test_unplaced_file_fails_by_name(self):
        files = dict(FILES, **{'core/zzz.h': ''})
        with self.assertRaises(SystemExit) as e:
            depgraph.analyze(tree(files), MODULES)
        self.assertIn('core/zzz.h', str(e.exception))

    def test_downward_include_passes(self):
        # high (level 1) includes low (level 0) in b.cpp and c.cpp: the only includes the levels allow
        bad = depgraph.level_violations(self.r, MODULES, LEVELS, [])
        self.assertEqual(sorted(bad), [('low', 'high')])
        self.assertEqual(bad[('low', 'high')], ['core/a_x.h:3'])

    def test_upward_include_fails(self):
        bad = depgraph.level_violations(self.r, MODULES, LEVELS, [])
        self.assertIn(('low', 'high'), bad)     # a_x.h:3 includes b.h, one level up

    def test_independent_pair_fails_either_way(self):
        bad = depgraph.level_violations(self.r, MODULES, LEVELS, [('low', 'high')])
        self.assertEqual(bad[('high', 'low')], ['core/b.cpp:2', 'core/c.cpp:2'])   # allowed by level, not by independence
        self.assertEqual(bad[('low', 'high')], ['core/a_x.h:3'])

    def test_levels_and_modules_must_match(self):
        with self.assertRaises(SystemExit) as e:
            depgraph.level_violations(self.r, MODULES, LEVELS[:1], [])
        self.assertIn('high', str(e.exception))

    def test_allowlist_count_must_be_exact(self):
        bad = {('low', 'high'): ['core/a_x.h:3']}
        self.assertEqual(depgraph.level_problems(bad, {('low', 'high'): (1, 'W1')}), [])
        above = depgraph.level_problems(bad, {('low', 'high'): (0, 'W1')})
        self.assertEqual(len(above), 1)
        self.assertIn('core/a_x.h:3', above[0])
        below = depgraph.level_problems(bad, {('low', 'high'): (2, 'W1')})
        self.assertEqual(below, ['lower the allowlist for low -> high to 1'])
        self.assertEqual(len(depgraph.level_problems({}, {('low', 'high'): (1, 'W1')})), 1)
        self.assertEqual(len(depgraph.level_problems(bad, {})), 1)


if __name__ == '__main__':
    unittest.main()
