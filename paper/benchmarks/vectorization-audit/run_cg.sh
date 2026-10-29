#!/bin/bash
# usage: run_cg.sh NAME MODEL   -> out/NAME.cg and out/NAME.top (inclusive-off self cost, top 25)
# runs ./xppautX MODEL --silent under callgrind in a scratch cwd (output.dat lands there)
set -e
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd)
BIN=${BIN:-$ROOT/xppautX}
T=$(mktemp -d); cd "$T"
valgrind --tool=callgrind --callgrind-out-file="$HERE/out/$1.cg" "$BIN" "$2" --silent >/dev/null 2>"$T/err" || true
callgrind_annotate "$HERE/out/$1.cg" 2>/dev/null | sed -n '1,45p' > "$HERE/out/$1.top"
rm -rf "$T"
