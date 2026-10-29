#!/bin/bash
# Foreground, untimed stage-1 gates, under tools/wslrun.sh's heavy slot.
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
mkdir -p build/w378-study
cp "$source_dir"/*.py "$source_dir"/*.cpp "$source_dir"/*.h build/w378-study/
uv run --locked python -m py_compile build/w378-study/*.py
uv run --locked python build/w378-study/test_protocol.py
make -j4 xppautx test BUILDDIR=build/w378/core OPT='-g -O2 -fPIC' WINDOW=0
