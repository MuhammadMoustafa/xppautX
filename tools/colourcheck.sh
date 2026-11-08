#!/bin/sh
# One spelling in user text (W642): see tools/colourcheck.py.
# Usage: tools/colourcheck.sh [--check]
cd "$(dirname "$0")/.." || exit 1
exec ${PYTHON:-uv run --locked python} tools/colourcheck.py "$@"
