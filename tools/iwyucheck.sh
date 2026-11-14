#!/bin/sh
# include-what-you-use over core/ and tests/ (W729): see tools/iwyucheck.py.
# Usage: tools/iwyucheck.sh [--jobs N] [--only STEM,...] [--db-only]
cd "$(dirname "$0")/.." || exit 1
. tools/jobs.sh
export JOBS
exec ${PYTHON:-uv run --locked python} tools/iwyucheck.py "$@"
