#!/bin/sh
# The manual names only what a user sees (W643): see tools/manualcheck.py.
# Usage: tools/manualcheck.sh [--check]
cd "$(dirname "$0")/.." || exit 1
exec ${PYTHON:-uv run --locked python} tools/manualcheck.py "$@"
