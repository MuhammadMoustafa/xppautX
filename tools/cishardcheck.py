#!/usr/bin/env python3
"""W169: every autocheck section belongs to exactly one sanitizer half."""
import collections
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parent.parent
BALANCE_DIFFERENCE = 1  # An odd section count puts one extra section in one half.
sections = subprocess.check_output(
    [sys.executable, str(root / 'tools/autocheck.py'), '--list'], text=True).split()
workflow = (root / '.github/workflows/build.yml').read_text(encoding='utf-8')
shards = [names.split('+') for names in re.findall(r'only: build,autocheck=([^\s]+)', workflow)]
expected = collections.Counter(sections)
actual = collections.Counter(name for shard in shards for name in shard)
if actual != expected or not shards or max(map(len, shards)) - min(map(len, shards)) > BALANCE_DIFFERENCE:
    sys.exit('CI autocheck shards must cover --list exactly once in balanced halves: ' + str(shards))
print('CI shards: %d sections, %s, all passed' % (len(sections), '/'.join(str(len(s)) for s in shards)))
