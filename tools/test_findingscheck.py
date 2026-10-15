#!/usr/bin/env python3
"""Self-test of tools/findingscheck.py (W393, #455): a small bad findings file
in a temp folder fails with each message, a good one passes."""
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import findingscheck  # noqa: E402

GOOD = """# Findings

| # | Area | Finding | Card |
|---|---|---|---|
| [1](#1-first-entry) | A | one | W1 |
| [2](#2-second-entry-with-code-and-a-dot) | B | two | W2 |

## 1. First entry

Text.

## 2. Second entry with `code` and a dot.

Text.
"""

# each case: the bad file's lines, and the message it must produce
CASES = {
    'duplicate': (
        GOOD.replace('## 2. Second entry with `code` and a dot.',
                     '## 1. Second entry with `code` and a dot.'),
        'entry 1 is already a heading at line'),
    'gap': (
        GOOD.replace('## 2.', '## 3.').replace('[2](#2-', '[3](#3-'),
        'entry 3 is out of order: expected 2'),
    'row without entry': (
        GOOD.replace('| [2](#2-second-entry-with-code-and-a-dot) | B | two | W2 |',
                     '| [2](#2-second-entry-with-code-and-a-dot) | B | two | W2 |\n'
                     '| [3](#3-third) | C | three | W3 |'),
        'index row 3 has no entry'),
    'entry without row': (
        GOOD.replace('| [2](#2-second-entry-with-code-and-a-dot) | B | two | W2 |\n', ''),
        'entry 2 has no index row'),
    'wrong anchor': (
        GOOD.replace('[2](#2-second-entry-with-code-and-a-dot)', '[2](#2-second-entry)'),
        'index row 2 links to #2-second-entry, its entry is at'),
    'repeated row': (
        GOOD.replace('| [2](#2-', '| [1](#1-first-entry) | A | again | W1 |\n| [2](#2-'),
        'index row 1 is already listed at line'),
}


class FindingsCheck(unittest.TestCase):
    def write(self, folder, text):
        path = os.path.join(folder, 'findings.md')
        with open(path, 'w', encoding='utf-8', newline='') as f:
            f.write(text)
        return path

    def test_good_file_passes(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(findingscheck.check(self.write(folder, GOOD)), [])

    def test_each_failure_is_named(self):
        with tempfile.TemporaryDirectory() as folder:
            for name, (text, message) in CASES.items():
                with self.subTest(name):
                    problems = findingscheck.check(self.write(folder, text))
                    self.assertTrue(any(message in p for p in problems), problems)
                    self.assertTrue(all(p.startswith(os.path.join(folder, 'findings.md') + ':')
                                        for p in problems), problems)

    def test_command_exits_one_with_file_and_line(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.write(folder, CASES['entry without row'][0])
            run = subprocess.run([sys.executable, os.path.join(HERE, 'findingscheck.py'), path],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 1)
            self.assertIn('findings.md:', run.stdout)
            self.assertIn('entry 2 has no index row', run.stdout)


if __name__ == '__main__':
    unittest.main()
