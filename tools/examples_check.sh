#!/bin/sh
# Run every example ODE through xppautX --silent (tools/run_example.sh, one
# per model, several at once) and compare what each writes (output.dat's
# md5 with CRs removed, "none" when the model writes nothing by itself,
# "noload" when it does not load by itself: includes, DLLs) with a
# baseline. A model that crashes (another nonzero exit) or
# times out fails the check whatever the baseline says.
#
# A difference means the numerics changed: find why, and only when the
# change is intended, rewrite the baseline with --update and say so in the
# commit message.
#
# One baseline, tests/examples.md5, for every platform (W159, W235): a
# difference on any of them is a bug to trace, never a baseline of its own.
#
# Usage: tools/examples_check.sh [--update] [--bin PATH] [--baseline FILE]
#                                [--write FILE] [--keep DIR] [--no-compile]
#   --bin PATH       the binary (default ./xppautX, or ./xppautX.exe)
#   --no-compile     run the interpreter against the same numerical baseline
#   --baseline FILE  compare with FILE (default tests/examples.md5)
#   --write FILE     also write the md5s computed here to FILE
#   --update         write the md5s computed here as the baseline
#   --keep DIR       put the output.dat of each model that differs from the
#                    baseline in DIR (as <path with / made _>.dat), to see
#                    what changed, and the diff as DIR/differs.txt (CI
#                    uploads DIR and the md5s only when DIR exists)
# TIMEOUT=seconds per run (default 300: the slowest model takes ~12 s
# alone, several times that while other checks share the machine).
# JOBS=models run at once (default: the machine's core count).
cd "$(dirname "$0")/.." || exit 1
top=$PWD
base=tests/examples.md5
write=
keep=
update=0
bin=
no_compile=
while [ $# -gt 0 ]; do
  case "$1" in
    --no-compile) no_compile=--no-compile ;;
    --update) update=1 ;;
    --bin) bin=$2; shift ;;
    --baseline) base=$2; shift ;;
    --write) write=$2; shift ;;
    --keep) keep=$2; shift ;;
    *) echo "examples_check.sh: unknown option $1"; exit 2 ;;
  esac
  shift
done
if [ -z "$bin" ]; then
  if [ ! -e xppautX ] && [ -e xppautX.exe ]; then bin=xppautX.exe; else bin=xppautX; fi
fi
case "$bin" in /*) ;; *) bin=$top/$bin ;; esac
jobs=${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
out=$(mktemp -d)
[ -n "$keep" ] && export KEEP_OUTPUT=1
find examples -name '*.odex' -o -name '*.ode' | while read -r f; do
  # The retained foreign fixtures are tested end to end by odexcheck.
  case "$f" in *.ode) [ -e "${f%.ode}.odex" ] && continue ;; esac
  echo "$f"
done | sort \
  | xargs -P"$jobs" -I{} tools/run_example.sh {} "$bin" "${TIMEOUT:-300}" "$out" "$no_compile" > "$out/failures" 2>&1
cat "$out"/*.sum | LC_ALL=C sort -k2 > "$out/all"
n=$(wc -l < "$out/all" | tr -d ' ')
if [ -n "$write" ]; then cp "$out/all" "$write"; echo "md5s written to $write"; fi
# a model that crashed or timed out (run_example.sh printed why)
failed=$(grep -l -v '^0$' "$out"/*.st 2>/dev/null | wc -l | tr -d ' ')
if [ "$failed" -ne 0 ]; then
  cat "$out/failures"
  rm -rf "$out"
  echo "EXAMPLES FAILED: $failed of $n models crashed or timed out"
  exit 1
fi
if [ $update -eq 1 ]; then
  mv "$out/all" "$base"
  rm -rf "$out"
  echo "examples baseline written to $base: $n models, $(grep -vc '^none' "$base") with output"
  exit 0
fi
bad=$(LC_ALL=C sort -k2 "$base" | diff - "$out/all" | grep '^[<>]')
if [ -n "$keep" ] && [ -n "$bad" ]; then
  mkdir -p "$keep"
  echo "$bad" > "$keep/differs.txt"   # the diff itself: CI uploads when it exists
  echo "$bad" | sed -n 's/^> [^ ]* //p' | while read -r f; do
    name=$(echo "$f" | tr / _)
    [ -e "$out/$name.dat" ] && cp "$out/$name.dat" "$keep/"
  done
  echo "the differing models' output.dat: $keep"
fi
rm -rf "$out"
if [ -n "$bad" ]; then
  echo "$bad"
  ndiff=$(echo "$bad" | grep -c '^>')
  echo "EXAMPLES DIFFER from $base: $ndiff of $n models"
  exit 1
fi
# the models that write nothing by themselves (a range run, say), named so
# "183 of 184" reads as all of them rather than one failing
nonef=$(grep '^none' "$base" | sed 's/^none //; s#.*/##' | tr '\n' ' ')
echo "examples ok: $n models, $(grep -vc '^none' "$base") outputs match $base${nonef:+; no output by design: $nonef}"
