#!/bin/sh
# One option, one row (W293): see tools/optioncheck.py.
# Usage: tools/optioncheck.sh [--check|--update|--list]
cd "$(dirname "$0")/.." || exit 1
exec ${PYTHON:-uv run --locked python} tools/optioncheck.py "$@"
