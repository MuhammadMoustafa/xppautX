#!/bin/bash
# W9: builds the WebAssembly core and the page's files, then runs
# tools/wasmcheck.mjs against the native program (docs/wasm.md). Needs
# emsdk (`git clone https://github.com/emscripten-core/emsdk ~/emsdk`,
# `./emsdk install latest && ./emsdk activate latest`: under the home folder,
# no sudo); its node runs the check. From WSL's own copy:
#   tools/wslrun.sh tools/wasmcheck.sh [--bench]
# Not in verify.sh or CI: they have no emsdk (the card's report says what
# adding it would take).
set -eu
cd "$(dirname "$0")/.."
EMSDK="${EMSDK:-$HOME/emsdk}"
if ! command -v emcc >/dev/null 2>&1; then
  [ -f "$EMSDK/emsdk_env.sh" ] || { echo "wasmcheck: no emsdk at $EMSDK" >&2; exit 2; }
  # shellcheck disable=SC1091
  . "$EMSDK/emsdk_env.sh" >/dev/null 2>&1
fi
JOBS="${JOBS:-3}"
make -s -j"$JOBS" wasm
make -s -j"$JOBS" xppautx
[ -d web2/node_modules ] || (cd web2 && npm ci --no-audit --no-fund >/dev/null)
node wasm/build.mjs --typecheck
node tools/wasmcheck.mjs --native ./xppautX "$@"
