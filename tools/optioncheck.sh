#!/bin/sh
# One option, one row (W293): see tools/optioncheck.py.
# Usage: tools/optioncheck.sh [--check|--update|--list]
cd "$(dirname "$0")/.." || exit 1
exec "${PYTHON:-python3}" tools/optioncheck.py "$@"
