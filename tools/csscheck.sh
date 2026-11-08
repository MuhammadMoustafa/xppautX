#!/bin/sh
# One rule per thing in web2's CSS (W638): see tools/csscheck.py.
# Usage: tools/csscheck.sh [--check]
cd "$(dirname "$0")/.." || exit 1
exec ${PYTHON:-uv run --locked python} tools/csscheck.py "$@"
