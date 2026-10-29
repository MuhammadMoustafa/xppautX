#!/bin/bash
# W543: tracked inputs need no copy into a verification clone.
set -euo pipefail
cd "$(dirname "$0")/../../.."
exec uv run --locked python tools/comparisoncheck.py "$@"
