#!/usr/bin/env python3
"""Self-test of tools/depgraph.py (W725, #803) on a tiny tree: a header cycle, a module
cycle, reach, a skipped `#if 0` include and an unplaced file."""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depgraph  # noqa: E402

MODULES = {'low': ('the owner', ['a', 'a_*']), 'high': ('the user', ['b*', 'c*'])}


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


if __name__ == '__main__':
    unittest.main()
