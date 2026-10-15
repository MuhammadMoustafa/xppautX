#!/bin/sh
# W358 (#420): xppautX under gdb, for tools/auto-profile/profile.py --tool gdb (the program is
# started like callgrind_wrap.sh's). GDB_SCRIPT is dump_matrices.py. gdb's own messages share
# stdout with the program's protocol lines, so the ones it prints on its own are switched off
# (no debuginfod, no thread-library or thread-event notes).
exec gdb -batch -q -iex 'set debuginfod enabled off' -iex 'set libthread-db-search-path /nonexistent' \
    -ex 'set print thread-events off' -ex 'set confirm off' -ex 'set pagination off' \
    -ex 'handle SIGPIPE nostop noprint pass' -x "${GDB_SCRIPT:?}" --args "${XPP_PROFILED:?}" "$@"
