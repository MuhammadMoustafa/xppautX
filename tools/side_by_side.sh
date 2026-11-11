#!/bin/sh
# Run independent checks side by side (W41, W694): each in the background with
# its own log, all waited for, then one verdict line each (in the order given),
# the tail of a failed check's log, and a non-zero exit if any failed. The one
# owner of that pattern: tools/asancheck.sh and CI's core jobs call it.
# Usage: tools/side_by_side.sh [--prefix PREFIX] [--jobs N] NAME COMMAND [ARGS...] [-- NAME COMMAND ...]
#   --prefix PREFIX  each check's log is PREFIX<name>.log (default build/side-by-side-)
#   --jobs N         at most N checks run at once, started in the order given
#                    (the next starts as soon as any running one has ended);
#                    default: all at once
# Prints "NAME finished after Ns" as each one exits (the long pole shows in a
# job's log), then "NAME ok" ("NAME ok: N checks" counting the log's ^PASS lines)
# or the log's non-PASS tail and "NAME FAILED". XPP_CHECK_SLOW and the rest of
# the environment pass to every command unchanged.
usage="usage: side_by_side.sh [--prefix PREFIX] [--jobs N] NAME COMMAND [ARGS...] [-- NAME COMMAND ...]"
prefix=build/side-by-side-
jobs=0
while [ $# -ge 1 ]; do
  case "$1" in
    --prefix) [ $# -ge 2 ] || { echo "$usage"; exit 2; }; prefix=$2; shift 2 ;;
    --jobs)
      [ $# -ge 2 ] || { echo "$usage"; exit 2; }
      case "$2" in ''|*[!0-9]*|0) echo "side_by_side.sh: --jobs wants a positive number"; exit 2 ;; esac
      jobs=$2; shift 2 ;;
    *) break ;;
  esac
done
[ $# -ge 2 ] || { echo "$usage"; exit 2; }
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

# --jobs: a slot is a line in a pipe; a check takes one before it starts and gives
# it back when it ends, so the next starts as soon as any one has ended
if [ "$jobs" -gt 0 ]; then
  slots=$(mktemp -u "${TMPDIR:-/tmp}/side-by-side.XXXXXX") && mkfifo "$slots" || exit 1
  exec 3<>"$slots"
  rm -f "$slots"
  i=0
  while [ $i -lt "$jobs" ]; do echo >&3; i=$((i + 1)); done
fi
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
  [ "$jobs" -gt 0 ] && read -r _ <&3
  # the group's first n words are the command: rotate them behind the rest, then drop the rest
  (
    i=0
    while [ $i -lt $n ]; do set -- "$@" "$1"; shift; i=$((i + 1)); done
    drop=$(( $# - n ))
    while [ $drop -gt 0 ]; do shift; drop=$((drop - 1)); done
    run_check "$name" "$@"
    [ "$jobs" -gt 0 ] && echo >&3
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
