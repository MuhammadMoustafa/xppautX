#!/bin/sh
# One thing, one name (W113): see tools/aliascheck.py.
# Usage: tools/aliascheck.sh [--check]
cd "$(dirname "$0")/.." || exit 1
exec "${PYTHON:-python3}" tools/aliascheck.py "$@"
