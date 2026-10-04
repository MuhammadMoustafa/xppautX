#!/usr/bin/env python3
"""W169/W181: every AUTO section belongs to one sanitizer shard per platform."""
import collections
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parent.parent
BALANCE_DIFFERENCE = 1  # Round-robin partition sizes differ by at most one section.
sections = subprocess.check_output(
    [sys.executable, str(root / 'tools/autocheck.py'), '--list'], text=True).split()
workflow = (root / '.github/workflows/build.yml').read_text(encoding='utf-8')
expected = collections.Counter(sections)
jobs = dict(re.findall(r'^  ([\w-]+):\n(.*?)(?=^  [\w-]+:|\Z)', workflow, re.M | re.S))
for job in ['linux-sanitizers', 'windows-clang-sanitizers']:
    selectors = re.findall(r'only: build,autocheck-shard=([^\s]+)', jobs.get(job, ''))
    shards = [subprocess.check_output(
        [sys.executable, str(root / 'tools/autocheck.py'), '--list', '--shard', selector],
        text=True).split() for selector in selectors]
    actual = collections.Counter(name for shard in shards for name in shard)
    if actual != expected or not shards or max(map(len, shards)) - min(map(len, shards)) > BALANCE_DIFFERENCE:
        sys.exit(job + ': AUTO shards must cover --list exactly once in balanced partitions: ' + str(shards))
    print('%s: %d AUTO sections, %s, all passed' % (
        job, len(sections), '/'.join(str(len(s)) for s in shards)))
