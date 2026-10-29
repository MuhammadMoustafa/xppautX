#!/bin/bash
# usage: exp.sh NAME "EXTRA CXXFLAGS" core/a.cpp [core/b.cpp...]
# Scratch experiment: compiles the named sources (as they are in the working tree) with the extra flags into
# scratch/exp-NAME/, and links scratch/exp-NAME/xppautX from build/obj plus those objects (earlier objects win over the
# archive's). Nothing under build/ or core/ is touched.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd); cd "$ROOT"
N=$1; X=$2; shift 2; D=$HERE/scratch/exp-$N; mkdir -p $D
BASE="-std=c++23 -g -O2 -ffp-contract=off -DNOERRNO -DNON_UNIX_STDIO -DAUTO -DCVODE_YES -DMYSTR1=8.0 -DMYSTR2=1 -Icore"
OBJS=""
for f in "$@"; do o=$D/$(basename $f .cpp).o; g++ $BASE $X -c $f -o $o; OBJS="$OBJS $o"; done
MAINS=$(ls build/obj/{ui_json,json_io,json_prompts,json_record,json_player,json_state,json_windows,json_auto,json_ani,json_model,json_silent,xppautx_main,xpp_about,xpp_http,xpp_inbox,web_assets,xpp_window}.o)
g++ -o $D/xppautX $MAINS $OBJS build/obj/libxppcore.a -lm -ldl -lpthread
echo $D/xppautX
