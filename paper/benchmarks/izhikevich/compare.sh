#!/bin/sh
# Redo W416's comparison (run in WSL, from any folder):
#   compare.sh XPPAUT_BINARY XPPAUTX_BINARY [ODEX_FOLDER]
# Each izh_*.ode is copied to its own temporary folder and run by XPPAUT 8.0 (-silent) and by
# xppautX (--silent, which also converts it to .odex there). output.dat must be identical byte for
# byte (CRs removed), and, with ODEX_FOLDER (the repository's examples/ode), the converted .odex
# must be the committed one byte for byte. Prints the two counts; exits 1 on any difference.
set -u
[ $# -ge 2 ] || { echo "usage: compare.sh XPPAUT XPPAUTX [ODEX_FOLDER]"; exit 2; }
here=$(cd "$(dirname "$0")" && pwd)
xpp=$(readlink -f "$1"); xx=$(readlink -f "$2"); odexdir=${3:-}
same=0 total=0 converted=0
for f in "$here"/izh_*.ode; do
  n=$(basename "$f" .ode); total=$((total + 1))
  a=$(mktemp -d); b=$(mktemp -d)
  cp "$f" "$a/"; cp "$f" "$b/"
  (cd "$a" && "$xpp" "$n.ode" -silent >/dev/null 2>&1)
  (cd "$b" && "$xx" "$n.ode" --silent >/dev/null 2>&1)
  if [ -s "$a/output.dat" ] && [ "$(tr -d '\r' < "$a/output.dat" | md5sum)" = "$(tr -d '\r' < "$b/output.dat" | md5sum)" ]; then
    same=$((same + 1))
  else
    echo "DIFFERENT output: $n"
  fi
  if [ -n "$odexdir" ]; then
    if cmp -s "$b/$n.odex" "$odexdir/$n.odex"; then converted=$((converted + 1)); else echo "DIFFERENT odex: $n"; fi
  fi
  rm -rf "$a" "$b"
done
echo "output identical: $same of $total"
[ -n "$odexdir" ] && echo "converted .odex identical: $converted of $total"
[ "$same" -eq "$total" ] && { [ -z "$odexdir" ] || [ "$converted" -eq "$total" ]; }
