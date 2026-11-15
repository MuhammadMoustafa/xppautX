#!/usr/bin/env python3
"""Self-test of tools/linkcheck.py (W746, #825) on small fake `nm -g -P` outputs: a reference
to a lower module's definition passes, one to a higher module's fails, a weak or own definition
is the object's own, a symbol no core object defines is ignored, an independent pair fails
either way, and the allowlist's count is exact."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import depgraph  # noqa: E402
import linkcheck  # noqa: E402

MODULES = {'low': ('the owner', ['a*']), 'high': ('the user', ['b*']), 'side': ('a partner', ['c*'])}
LEVELS = [('low', 'kernel', 'lower'), ('high', 'domain model', 'higher'), ('side', 'outside', 'a partner of high')]


def edges(objects):
    return linkcheck.link_edges(objects, MODULES)


class Linkcheck(unittest.TestCase):
    def setUp(self):
        self.levels, self.indep = depgraph.LEVELS, depgraph.INDEPENDENT
        depgraph.LEVELS, depgraph.INDEPENDENT = LEVELS, [('high', 'side')]

    def tearDown(self):
        depgraph.LEVELS, depgraph.INDEPENDENT = self.levels, self.indep

    def test_read_nm_splits_kinds(self):
        strong, weak, undef = linkcheck.read_nm('f T 0 1\ng W 0 0\nh U\ni V 0 0\nj D 0 8\nk B 1 1\n')
        self.assertEqual((strong, weak, undef), ({'f', 'j', 'k'}, {'g', 'i'}, {'h'}))

    def test_downward_reference_passes(self):
        objs = {'core/a1.cpp': 'f T 0 1\n', 'core/b1.cpp': 'f U\n'}
        self.assertEqual(edges(objs), {})

    def test_upward_reference_fails_and_names_symbol(self):
        objs = {'core/a1.cpp': 'f U\n', 'core/b1.cpp': 'f T 0 1\n'}
        self.assertEqual(edges(objs), {('low', 'high'): ['core/a1.cpp:f']})

    def test_own_weak_and_foreign_symbols_are_not_edges(self):
        objs = {'core/a1.cpp': 'own T 0 1\nown U\ninl W 0 0\ninl U\nstd::x U\n', 'core/b1.cpp': 'inl T 0 1\n'}
        self.assertEqual(edges(objs), {})

    def test_weak_definition_elsewhere_is_no_edge(self):
        objs = {'core/a1.cpp': 'tmpl U\n', 'core/b1.cpp': 'tmpl W 0 0\n'}
        self.assertEqual(edges(objs), {})

    def test_same_module_is_no_edge(self):
        objs = {'core/a1.cpp': 'f U\n', 'core/a2.cpp': 'f T 0 1\n'}
        self.assertEqual(edges(objs), {})

    def test_independent_pair_fails_both_ways(self):
        objs = {'core/b1.cpp': 'f U\ng T 0 1\n', 'core/c1.cpp': 'g U\nf T 0 1\n'}
        bad = edges(objs)
        self.assertEqual(sorted(bad), [('high', 'side'), ('side', 'high')])

    def test_allowlist_count_is_exact(self):
        bad = {('low', 'high'): ['core/a1.cpp:f']}
        pr = lambda allow: depgraph.level_problems(bad, allow, 'link edge(s)')
        self.assertEqual(pr({('low', 'high'): (1, 'W1')}), [])
        self.assertIn('core/a1.cpp:f', pr({('low', 'high'): (0, 'W1')})[0])
        self.assertEqual(pr({('low', 'high'): (2, 'W1')}), ['lower the allowlist for low -> high to 1'])
        self.assertEqual(len(pr({})), 1)

    def test_the_real_allowlist_names_real_modules_and_cards(self):
        names = {m for m, _, _ in self.levels}
        for (a, b), (n, card) in linkcheck.ALLOWLIST.items():
            self.assertTrue({a, b} <= names and n > 0 and card.startswith('W'), (a, b))


if __name__ == '__main__':
    unittest.main()
