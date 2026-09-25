#!/bin/sh
# Run one example .ode model headless (xppautX -silent), in a temp dir of
# its own holding a copy of the model's folder. tools/examples_check.sh
# runs this once per model, several at once through xargs -P (W16, W17).
#
# Usage: tools/run_example.sh <model.ode> <xppautX-binary> [timeout-seconds] [result-dir]
#
# Exit status: 0 if the model ran to completion (exit 0) within the
# timeout, 1 otherwise (prints "FAIL <model> exit <status>" or "FAIL
# <model> timeout" and the model's log). With a result dir it also writes
# there, under the model's path with / made _, a .sum file holding
# "<md5> <model>" (the md5 of output.dat with its CRs removed, so a
# Windows build's text-mode line ends hash like Linux's; "none" when the
# model writes nothing by itself: AUTO tests, includes; "timeout") and a
# .st file holding the exit status (or "timeout").
# The timeout is coreutils' timeout where there is one (Linux, Git Bash),
# else (macOS) a watcher that kills the run.
set -u
f=$1
bin=$2
timeout_s=${3:-300}
result=${4:-}

md5() {
  if command -v md5sum >/dev/null 2>&1; then md5sum | cut -d' ' -f1; else md5 -q; fi
}

run=$(mktemp -d)
cp "$(dirname "$f")"/* "$run"/ 2>/dev/null
rm -f "$run/output.dat"
st=0
timed_out=0
tmo=
# (--version: never Windows' own timeout.exe, which only waits)
if timeout --version >/dev/null 2>&1; then tmo=timeout
elif gtimeout --version >/dev/null 2>&1; then tmo=gtimeout
fi
if [ -n "$tmo" ]; then
  ( cd "$run" && exec "$tmo" "$timeout_s" "$bin" "$(basename "$f")" -silent >run.log 2>&1 ) || st=$?
  [ "$st" -eq 124 ] && timed_out=1
else
  ( cd "$run" && exec "$bin" "$(basename "$f")" -silent >run.log 2>&1 ) &
  pid=$!
  ( sleep "$timeout_s" && touch "$run/.timed_out" && kill -9 "$pid" ) >/dev/null 2>&1 &
  watcher=$!
  wait "$pid" || st=$?
  pkill -P "$watcher" 2>/dev/null
  kill "$watcher" 2>/dev/null
  wait "$watcher" 2>/dev/null
  [ -e "$run/.timed_out" ] && timed_out=1
fi
if [ $timed_out -eq 1 ]; then
  sum=timeout
  st=timeout
elif [ -s "$run/output.dat" ]; then
  sum=$(tr -d '\r' < "$run/output.dat" | md5)
else
  sum=none
fi
if [ -n "$result" ]; then
  name=$(echo "$f" | tr / _)
  echo "$sum $f" > "$result/$name.sum"
  echo "$st" > "$result/$name.st"
fi
if [ "$st" != 0 ]; then
  if [ "$st" = timeout ]; then echo "FAIL $f timeout ($timeout_s s)"; else echo "FAIL $f exit $st"; fi
  tail -20 "$run/run.log"
  rm -rf "$run"
  exit 1
fi
rm -rf "$run"
exit 0
