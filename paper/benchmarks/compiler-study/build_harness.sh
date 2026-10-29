#!/bin/bash
# Builds ./harness: against the xppautX tree W (default: the study's worktree) and MIR M.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
W=${W:-/home/muhammad/muhammad/git_repos/xppautX-W472}
M=${M:-$HERE/mir-v1.0.0}
g++ -std=c++23 -O2 -ffp-contract=off -g -I"$W/core" -I"$W" -I"$M" "$HERE/harness.cpp" \
  "$W/build/obj/libxppcore.a" "$M/libmir.a" -lm -ldl -lpthread -rdynamic -o "$HERE/harness"
