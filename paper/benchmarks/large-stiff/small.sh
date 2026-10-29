#!/bin/bash
# Invoked by tools/wslrun.sh: copy ignored study inputs onto WSL's own disk.
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
mkdir -p build/w378-study
cp "$source_dir"/*.py "$source_dir"/*.cpp "$source_dir"/*.h build/w378-study/
uv run --locked python -m py_compile build/w378-study/*.py
uv run --locked python build/w378-study/build.py --sundials "$HOME/sundials-install" --out build/w378
uv run --locked python build/w378-study/study.py check --worker build/w378/worker --out "${1:-build/w378/correctness}" --allow-no-klu
