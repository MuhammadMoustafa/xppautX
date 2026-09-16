#!/bin/sh
# Run every example ODE through the X11 program (xppaut -silent) and the
# headless runner (xppcore-cli) and compare what they write. The two share
# the numerics, so a difference means the split broke something; a model
# only one of them can run at all is reported too.
# Usage: tools/examples_check.sh [pattern]     (default: every examples/**/*.ode)
# TIMEOUT=seconds per run (default 60), OUT=dir keeps the runs.
cd "$(dirname "$0")/.." || exit 1
top=$PWD
out=${OUT:-$top/build/examples}
timeout=${TIMEOUT:-60}
pattern=${1:-}
rm -rf "$out" && mkdir -p "$out" || exit 1
same=0; diff=0; neither=0; only=0; total=0
for f in $(find examples -name '*.ode' | sort); do
  case "$f" in
    *$pattern*) ;;
    *) continue ;;
  esac
  total=$((total + 1))
  name=$(echo "$f" | tr '/' '_')
  dir=$(dirname "$f")
  for tag in base new; do
    run=$out/$tag
    rm -rf "$run" && mkdir -p "$run"
    cp "$dir"/* "$run"/ 2>/dev/null
    if [ $tag = base ]; then bin="$top/xppaut"; set -- -silent; else bin="$top/xppcore-cli"; set --; fi
    ( cd "$run" && timeout "$timeout" "$bin" "$(basename "$f")" "$@" > run.log 2>&1 )
    echo $? > "$run/status"
  done
  a=$out/base/output.dat; b=$out/new/output.dat
  if [ -s "$a" ] && [ -s "$b" ]; then
    if cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      diff=$((diff + 1))
      echo "DIFF: $f ($(wc -l < "$a") vs $(wc -l < "$b") rows)"
      mkdir -p "$out/diffs/$name" && cp "$a" "$out/diffs/$name/base.dat" && cp "$b" "$out/diffs/$name/new.dat"
      cp "$out/base/run.log" "$out/diffs/$name/base.log"
      cp "$out/new/run.log" "$out/diffs/$name/new.log"
    fi
  elif [ -s "$a" ] || [ -s "$b" ]; then
    only=$((only + 1))
    [ -s "$a" ] && which=xppcore-cli || which=xppaut
    echo "ONLY ONE RAN: $f (no output from $which)"
    mkdir -p "$out/diffs/$name"
    cp "$out/base/run.log" "$out/diffs/$name/base.log"
    cp "$out/new/run.log" "$out/diffs/$name/new.log"
  else
    neither=$((neither + 1)) # not a model that runs by itself (AUTO tests, includes)
  fi
done
echo "examples: $total, identical output: $same, different: $diff, one-sided: $only, no output from either: $neither"
[ $diff -eq 0 ] && [ $only -eq 0 ]
