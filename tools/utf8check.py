#!/usr/bin/env python3
"""Every tracked text file is UTF-8 (tools/verify.sh runs this).

An editor on Windows can save a curly quote or an ellipsis as a single
Windows-1252 byte (0x85, 0x93, ...), which is not UTF-8: esbuild, tsc and
Python then refuse the file, or a merge carries the byte along unseen.
Files with a NUL byte in their first 8 KB count as binary and are skipped.
"""
import subprocess
import sys

bad = []
for name in subprocess.check_output(['git', 'ls-files', '-z']).decode('utf-8').split('\0'):
    if not name:
        continue
    try:
        data = open(name, 'rb').read()
    except OSError:
        continue
    if b'\0' in data[:8192]:
        continue
    try:
        data.decode('utf-8')
    except UnicodeDecodeError as e:
        bad.append('%s: byte 0x%02x at offset %d' % (name, data[e.start], e.start))
if bad:
    print('not UTF-8:\n  ' + '\n  '.join(bad))
    sys.exit(1)
print('utf-8 ok')
