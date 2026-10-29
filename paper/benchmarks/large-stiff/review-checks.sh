#!/bin/bash
# One foreground slot: build, short correctness, cap smoke tests, then unit gates.
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
tag=${1:-review}
bash "$source_dir/small.sh" "build/w378/correctness-$tag"
uv run --locked python build/w378-study/planning.py --worker build/w378/worker \
  --correctness "build/w378/correctness-$tag/correctness.json" \
  --out "build/w378/planning-$tag"
bash "$source_dir/gates.sh"
