#!/bin/sh
# One spelling in user text (W642): see tools/colorcheck.py.
# Usage: tools/colorcheck.sh [--check]
cd "$(dirname "$0")/.." || exit 1
exec ${PYTHON:-uv run --locked python} tools/colorcheck.py "$@"
