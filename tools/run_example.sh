#!/bin/sh
# Run one example .ode model headless, in its own temp dir, and report a
# crash. Split out of the macOS CI job's "every example runs without
# crashing" step (W16) so that step can run its models in parallel with
# xargs -P, one invocation of this script per model, instead of one
# `xppautX -silent` at a time.
#
# Usage: tools/run_example.sh <model.ode> <xppautX-binary> [timeout-seconds]
#
# Exit status: 0 if the model ran to completion (any exit code the model
# itself returns 0 for) within the timeout, 1 otherwise (prints "FAIL
# <model> exit <status>" and the model's log). Numerics are not checked
# here (that is Linux's job, tools/examples_check.sh via verify.sh).
set -u
f=$1
bin=$2
timeout_s=${3:-300}

run=$(mktemp -d)
cp "$(dirname "$f")"/* "$run"/ 2>/dev/null
( cd "$run" && "$bin" "$(basename "$f")" -silent >run.log 2>&1 ) &
pid=$!
( sleep "$timeout_s"; kill -9 "$pid" 2>/dev/null ) &
watcher=$!
st=0
wait "$pid" || st=$?
kill "$watcher" 2>/dev/null || true
wait "$watcher" 2>/dev/null || true
if [ "$st" -ne 0 ]; then
  echo "FAIL $f exit $st"
  cat "$run/run.log"
  rm -rf "$run"
  exit 1
fi
rm -rf "$run"
exit 0
