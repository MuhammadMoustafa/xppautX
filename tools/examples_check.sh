#!/bin/sh
# Run every example ODE through xppautX -silent and compare what it writes
# (output.dat's md5, or "none" when the model does not run by itself: AUTO
# tests, includes) with tests/examples.md5. A difference means the numerics
# changed: find why, and only when the change is intended, rewrite the
# baseline with --update and say so in the commit message.
# Usage: tools/examples_check.sh [--update]   (tools/verify.sh runs it)
# TIMEOUT=seconds per run (default 300: the slowest model takes ~12 s
# alone, several times that while other checks share the machine).
cd "$(dirname "$0")/.." || exit 1
top=$PWD
base=tests/examples.md5
out=$(mktemp -d)
find examples -name '*.ode' | sort | xargs -P"$(nproc 2>/dev/null || echo 4)" -I{} sh -c '
  f=$1; run=$2/$(echo "$f" | tr / _); mkdir -p "$run"
  cp "$(dirname "$f")"/* "$run"/ 2>/dev/null
  rm -f "$run/output.dat"
  ( cd "$run" && timeout "$4" "$3" "$(basename "$f")" -silent > run.log 2>&1 )
  st=$?
  if [ $st -eq 124 ]; then sum=timeout
  elif [ -s "$run/output.dat" ]; then sum=$(md5sum < "$run/output.dat" | cut -d" " -f1)
  else sum=none; fi
  echo "$sum $f" > "$run/sum"' sh {} "$out" "$top/xppautX" "${TIMEOUT:-300}"
cat "$out"/*/sum | sort -k2 > "$out/all"
rm -rf "$out"/*_*
if [ "$1" = --update ]; then
  mv "$out/all" "$base"
  rm -rf "$out"
  echo "examples baseline written: $(wc -l < "$base") models, $(grep -vc '^none' "$base") with output"
  exit 0
fi
bad=$(diff "$base" "$out/all" | grep '^[<>]')
rm -rf "$out"
if [ -n "$bad" ]; then
  echo "$bad"
  echo "EXAMPLES DIFFER from $base"
  exit 1
fi
echo "examples ok: $(wc -l < "$base") models, $(grep -vc '^none' "$base") outputs match the baseline"
