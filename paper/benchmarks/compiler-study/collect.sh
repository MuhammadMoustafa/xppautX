#!/bin/bash
# Dumps each model's generated C with the study patch (patch-dump-native.diff applied to a
# build of xppautX): data/<model>/{model0.c,model0.manifest,output.dat}.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
X=${X:-/home/muhammad/muhammad/git_repos/xppautX-W472/xppautX}
cd "$HERE"
for f in models/*.ode models/clustor.odex; do
  n=$(basename "$f"); n=${n%.*}
  [ -f "$f" ] || continue
  mkdir -p data/$n; (cd data/$n && XPP_DUMP_NATIVE=$PWD "$X" ../../$f --silent >log.txt 2>&1)
done
