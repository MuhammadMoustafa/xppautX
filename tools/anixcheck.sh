#!/bin/sh
# W520: compare the retained imports' compiled frame data with native .anix.
# make xppautx test builds this test program. Override its path for another build.
set -eu
cd "$(dirname "$0")/.."
exec "${1:-./build/obj/tests/test_anix}"
