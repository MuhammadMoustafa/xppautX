#!/bin/sh
# Run independent checks side by side (W41, W694): each in the background with
# its own log, all waited for, then one verdict line each (in the order given),
# the tail of a failed check's log, and a non-zero exit if any failed. The one
# owner of that pattern: tools/asancheck.sh and CI's core jobs call it.
# Usage: tools/side_by_side.sh [--prefix PREFIX] NAME COMMAND [ARGS...] [-- NAME COMMAND ...]
#   --prefix PREFIX  each check's log is PREFIX<name>.log (default build/side-by-side-)
# Prints "NAME finished after Ns" as each one exits (the long pole shows in a
# job's log), then "NAME ok" ("NAME ok: N checks" counting the log's ^PASS lines)
# or the log's non-PASS tail and "NAME FAILED". XPP_CHECK_SLOW and the rest of
# the environment pass to every command unchanged.
prefix=build/side-by-side-
if [ "$1" = --prefix ]; then
  [ $# -ge 2 ] || { echo "usage: side_by_side.sh [--prefix PREFIX] NAME COMMAND [ARGS...] [-- NAME COMMAND ...]"; exit 2; }
  prefix=$2; shift 2
fi
[ $# -ge 2 ] || { echo "usage: side_by_side.sh [--prefix PREFIX] NAME COMMAND [ARGS...] [-- NAME COMMAND ...]"; exit 2; }
mkdir -p "$(dirname "$prefix")" || exit 1

# run_check NAME COMMAND...: the command to its log, then the verdict file
run_check() {
  name=$1; shift
  t0=$(date +%s)
  if "$@" > "$prefix$name.log" 2>&1; then
    passes=$(grep -c '^PASS' "$prefix$name.log")
    if [ "$passes" -gt 0 ]; then echo "$name ok: $passes checks"; else echo "$name ok"; fi > "$prefix$name.verdict"
  else
    { grep -v '^PASS' "$prefix$name.log" | tail -40; echo "$name FAILED"; } > "$prefix$name.verdict"
  fi
  echo "$name finished after $(( $(date +%s) - t0 ))s"
}

names=
while [ $# -gt 0 ]; do
  name=$1; shift
  n=0
  for arg in "$@"; do
    [ "$arg" = -- ] && break
    n=$((n + 1))
  done
  [ "$n" -gt 0 ] || { echo "side_by_side.sh: $name has no command"; exit 2; }
  names="$names $name"
  # the group's first n words are the command: rotate them behind the rest, then drop the rest
  (
    i=0
    while [ $i -lt $n ]; do set -- "$@" "$1"; shift; i=$((i + 1)); done
    drop=$(( $# - n ))
    while [ $drop -gt 0 ]; do shift; drop=$((drop - 1)); done
    run_check "$name" "$@"
  ) &
  i=0
  while [ $i -lt $n ]; do shift; i=$((i + 1)); done
  [ $# -gt 0 ] && shift
done
wait
fail=0
for name in $names; do
  cat "$prefix$name.verdict"
  grep -q ' FAILED$' "$prefix$name.verdict" && fail=1
done
exit $fail
