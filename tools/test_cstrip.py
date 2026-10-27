#!/usr/bin/env python3
"""Self-test of tools/cstrip.py (W510, #574): a comment marker inside a
string or character literal is not a comment, in either stripper, and the
comments themselves go, with the line count kept."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cstrip  # noqa: E402


class StripComments(unittest.TestCase):
    def test_marker_inside_a_string_is_kept(self):
        src = 'const char *s="https://example"; // gone\n'
        self.assertEqual(cstrip.strip_comments(src), 'const char *s="https://example"; \n')

    def test_marker_inside_a_character_literal_is_kept(self):
        src = "case '/': /* gone */ x();\n"
        self.assertEqual(cstrip.strip_comments(src), "case '/':  x();\n")

    def test_block_comment_keeps_its_newlines(self):
        src = "a /* one\ntwo */ b\n"
        out = cstrip.strip_comments(src)
        self.assertEqual(out.count("\n"), src.count("\n"))
        self.assertNotIn("one", out)

    def test_strip_empties_literals(self):
        self.assertEqual(cstrip.strip('f("//x", \'/\'); // c\n'), 'f("", \'\'); \n')


if __name__ == "__main__":
    unittest.main()
