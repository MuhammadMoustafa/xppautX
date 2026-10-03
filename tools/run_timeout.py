#!/usr/bin/env python3
"""Run a command until it exits, with a safety ceiling (124 on timeout).

Shared by modecheck and run_example where coreutils timeout is unavailable.
subprocess.run waits on the child process and kills only that child on timeout.
"""
import subprocess
import sys

try:
    # Explicit streams preserve Git Bash's redirected handles on native Windows.
    result = subprocess.run(sys.argv[2:], timeout=float(sys.argv[1]),
                            stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr)
    sys.exit(result.returncode if result.returncode >= 0 else 128 - result.returncode)
except subprocess.TimeoutExpired:
    sys.exit(124)
