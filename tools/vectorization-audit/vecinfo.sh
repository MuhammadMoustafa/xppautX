#!/bin/sh
# W476 (#539): gcc's vectorizer notes (-fopt-info-vec-all) for core files, compiled with the normal flags
# (the Makefile's: -std=c++23 -g -O2 -ffp-contract=off, no -march) into OUT, the objects thrown away.
#   tools/vectorization-audit/vecinfo.sh OUT FILE.cpp...
# OUT/FILE.vec holds, per loop, "optimized: loop vectorized using N byte vectors" or "missed: couldn't
# vectorize loop" with gcc's reason, at file:line.
set -e
out=$1; shift
mkdir -p "$out"
for f in "$@"; do
  b=$(basename "$f" .cpp)
  echo "$f $b"
done | xargs -P 6 -n 2 sh -c 'g++ -std=c++23 -pedantic -Wall -g -O2 -ffp-contract=off -DMYSTR1=8.0 -DMYSTR2=1 -Icore -c "$0" -o /dev/null -fopt-info-vec-all="'"$out"'/$1.vec" 2>/dev/null'
