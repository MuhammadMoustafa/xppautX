#!/bin/sh
# Duplication audit of core/ (W30; sourcecheck.sh runs it with --check,
# a few seconds): finds duplicated functions, duplicated blocks of lines,
# a struct/typedef defined more than once, and a function declared in
# more than one header. See tools/dupcheck.py's own comment for the
# method and tools/deadcode.sh for the same allowlist style. python3
# does the actual work; this wrapper only picks it and forwards args.
# Usage: tools/dupcheck.sh [--check] [core files...]
cd "$(dirname "$0")/.." || exit 1
PYTHON=${PYTHON:-python3}
exec "$PYTHON" tools/dupcheck.py "$@"
