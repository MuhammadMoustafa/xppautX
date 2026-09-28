#!/bin/sh
# Every example as .odex (W74, docs/odex.md): each examples/**/*.ode that
# loads by itself (its tests/examples.md5 entry an md5 or "none") is
# converted in a temp copy of its folder (xppautX --convert --auto, which
# checks the .odex compiles to the .ode's own programs) and the .odex run
# (xppautX model.odex -silent); output.dat's md5, CRs removed, must be the
# .ode's own baseline entry. A model that does not load by itself
# ("noload": an include, a DLL) is skipped. tools/verify.sh runs this.
#
# Usage: tools/odexcheck.sh [--bin PATH] [--baseline FILE] [--keep DIR]
#   --bin PATH       the binary (default ./xppautX, or ./xppautX.exe)
#   --baseline FILE  the .ode md5s (default tests/examples.md5)
#   --keep DIR       put each failing model's .odex and log in DIR
# TIMEOUT=seconds per run (default 300); JOBS=models at once (default:
# the machine's core count).
set -u
cd "$(dirname "$0")/.." || exit 1
top=$PWD

md5() {
  if command -v md5sum >/dev/null 2>&1; then md5sum | cut -d' ' -f1; else md5 -q; fi
}

# one model: --one <binary> <result-dir> <timeout> <model.ode> <its md5>
if [ "${1:-}" = --one ]; then
  bin=$2 out=$3 tmo_s=$4 f=$5 want=$6
  name=$(echo "$f" | tr / _)
  run=$(mktemp -d)
  cp "$(dirname "$f")"/* "$run"/ 2>/dev/null
  rm -f "$run/output.dat" "$run"/*.odex
  base=$(basename "$f" .ode)
  tmo=
  if timeout --version >/dev/null 2>&1; then tmo="timeout $tmo_s"
  elif gtimeout --version >/dev/null 2>&1; then tmo="gtimeout $tmo_s"
  fi
  why=
  if ! ( cd "$run" && exec $tmo "$bin" --convert --auto "$base.ode" >convert.log 2>&1 ); then
    why="the conversion failed"
  else
    rm -f "$run/output.dat"
    st=0
    ( cd "$run" && exec $tmo "$bin" "$base.odex" -silent >run.log 2>&1 ) || st=$?
    if [ "$st" -ne 0 ]; then
      why="the .odex run exited $st"
    else
      if [ -s "$run/output.dat" ]; then got=$(tr -d '\r' < "$run/output.dat" | md5); else got=none; fi
      [ "$got" = "$want" ] || why="md5 $got, the .ode's is $want"
    fi
  fi
  if [ -n "$why" ]; then
    echo "FAIL $f: $why"
    tail -5 "$run/convert.log" "$run/run.log" 2>/dev/null | sed 's/^/    /'
    if [ -n "${KEEP:-}" ]; then
      cp "$run/$base.odex" "$KEEP/$name.odex" 2>/dev/null
      cat "$run/convert.log" "$run/run.log" > "$KEEP/$name.log" 2>/dev/null
    fi
    echo fail > "$out/$name.r"
  else
    echo ok > "$out/$name.r"
  fi
  rm -rf "$run"
  exit 0
fi

bin= base=tests/examples.md5 keep=
while [ $# -gt 0 ]; do
  case "$1" in
    --bin) bin=$2; shift ;;
    --baseline) base=$2; shift ;;
    --keep) keep=$2; shift ;;
    *) echo "odexcheck.sh: unknown option $1"; exit 2 ;;
  esac
  shift
done
if [ -z "$bin" ]; then
  if [ ! -e xppautX ] && [ -e xppautX.exe ]; then bin=xppautX.exe; else bin=xppautX; fi
fi
case "$bin" in /*) ;; *) bin=$top/$bin ;; esac
if [ -n "$keep" ]; then mkdir -p "$keep"; KEEP=$(cd "$keep" && pwd); export KEEP; fi
jobs=${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
out=$(mktemp -d)
grep -v '^noload ' "$base" | awk '{print $2, $1}' \
  | xargs -P"$jobs" -L1 "$top/tools/odexcheck.sh" --one "$bin" "$out" "${TIMEOUT:-300}"
n=$(ls "$out"/*.r 2>/dev/null | wc -l | tr -d ' ')
bad=$(grep -l fail "$out"/*.r 2>/dev/null | wc -l | tr -d ' ')
rm -rf "$out"
if [ "$bad" -ne 0 ] || [ "$n" -eq 0 ]; then
  echo "odexcheck: $bad of $n models FAIL as .odex"
  exit 1
fi
echo "odexcheck ok: all $n models give the same output.dat as .odex"
