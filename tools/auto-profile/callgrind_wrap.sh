#!/bin/sh
# W358 (#420): xppautX under callgrind, for tools/auto-profile/profile.py (it passes this script
# as the program to tools/xppclient.py's Server, which appends "--server MODEL").
# CALLGRIND_OUT is the folder the profile files go to, one per process (%p);
# --compress-* off so profile.py can read the files without callgrind's name table.
exec valgrind --tool=callgrind --callgrind-out-file="${CALLGRIND_OUT:?}/cg.%p.out" \
    --compress-strings=no --compress-pos=no --dump-instr=no --separate-threads=no \
    --error-exitcode=0 "${XPP_PROFILED:?}" "$@"
