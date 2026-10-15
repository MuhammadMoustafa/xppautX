#!/usr/bin/env python3
"""Checks that a findings file numbers its entries once (W393, #455; sourcecheck
runs it). docs/xppaut-findings.md and docs/auto07p-findings.md each hold an
index table whose rows link `[N](#N-slug)` to an entry section `## N. Title`.
Two agents on parallel branches once took the same next number, found only at
rebase, so this fails, naming the file and line, when:

  - two entry headings share a number
  - the entry numbers are not 1, 2, 3, ... in order (a gap or a repeat)
  - an index row's number has no entry
  - an entry has no index row
  - an index row's link is not its entry's heading anchor

Usage: tools/findingscheck.py [FILE ...]   (default: the two findings docs)
The self-test is tools/test_findingscheck.py.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_FILES = [os.path.join(HERE, '..', 'docs', name)
                 for name in ('xppaut-findings.md', 'auto07p-findings.md')]

HEADING = re.compile(r'^## (\d+)\. (.+?)\s*$')
INDEX_ROW = re.compile(r'^\| \[(\d+)\]\((#[^)]*)\)')
FENCE = re.compile(r'^\s*(```|~~~)')


def slug(heading):
    """GitHub's anchor for a heading: lower case, punctuation and backticks
    dropped, each space a hyphen."""
    text = re.sub(r'[^\w\- ]', '', heading.lower(), flags=re.ASCII)
    return '#' + text.replace(' ', '-')


def check(path):
    """Returns the findings for one file, each a 'path:line: message' string."""
    problems = []
    entries = {}  # number -> (line, anchor)
    rows = {}     # number -> (line, anchor the row links to)
    order = []    # entry numbers in file order
    fenced = False
    with open(path, encoding='utf-8') as f:
        lines = f.read().splitlines()
    for lineno, text in enumerate(lines, 1):
        if FENCE.match(text):
            fenced = not fenced
            continue
        if fenced:
            continue
        m = HEADING.match(text)
        if m:
            number = int(m.group(1))
            if number in entries:
                problems.append(f'{path}:{lineno}: entry {number} is already '
                                f'a heading at line {entries[number][0]}')
            else:
                entries[number] = (lineno, slug(text[3:]))
            order.append(number)
            continue
        m = INDEX_ROW.match(text)
        if m:
            number = int(m.group(1))
            if number in rows:
                problems.append(f'{path}:{lineno}: index row {number} is '
                                f'already listed at line {rows[number][0]}')
            else:
                rows[number] = (lineno, m.group(2))
    for expected, number in enumerate(order, 1):
        if number != expected:
            problems.append(f'{path}:{entries[number][0]}: entry {number} is '
                            f'out of order: expected {expected}')
            break
    for number, (lineno, anchor) in sorted(entries.items()):
        if number not in rows:
            problems.append(f'{path}:{lineno}: entry {number} has no index row')
    for number, (lineno, anchor) in sorted(rows.items()):
        if number not in entries:
            problems.append(f'{path}:{lineno}: index row {number} has no entry')
        elif anchor != entries[number][1]:
            problems.append(f'{path}:{lineno}: index row {number} links to '
                            f'{anchor}, its entry is at {entries[number][1]} '
                            f'(line {entries[number][0]})')
    return problems


def main(argv):
    files = argv or DEFAULT_FILES
    problems = []
    for path in files:
        problems.extend(check(path))
    for line in problems:
        print(line)
    if problems:
        print(f'FINDINGS NUMBERING: {len(problems)} problem(s)')
        return 1
    print(f'findings numbering: {len(files)} file(s) pass')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
