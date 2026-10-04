#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
# W185: isolated SDK-source experiments; no application or vendored math edits.
# Requires the baseline built by JOBS=4 tools/wasmcheck.sh and MPFR runtime.
mkdir -p build/w185
source "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
export EMCC_CORES=4
sdk="$HOME/emsdk/upstream/emscripten/system/lib/libc/musl"
cp "$sdk/src/math/fma.c" build/w185/fma-original.c
python3 - <<'PY'
from pathlib import Path
p=Path('build/w185/fma-original.c').read_text().replace('>>64-d','>>(64-d)').replace('<<64-d','<<(64-d)')
Path('build/w185/fma-original.c').write_text(p)
start=p.index('\tuint64_t t1,t2,t3;')
end=p.index('\n}',start)
p=p[:start]+'''\tunsigned __int128 product = (unsigned __int128)x * y;
\t*lo = (uint64_t)product;
\t*hi = (uint64_t)(product >> 64);'''+p[end:]
Path('build/w185/fma-int128.c').write_text(p)
PY
for variant in original int128 wide; do
  src=build/w185/fma-original.c
  flags=-O3
  if [ "$variant" != original ]; then src=build/w185/fma-int128.c; fi
  if [ "$variant" = wide ]; then flags='-O3 -mwide-arithmetic'; fi
  emcc $flags -Wall -Wextra -Werror -ffp-contract=off -I"$sdk/src/internal" -I"$sdk/arch/emscripten" -c "$src" -o "build/w185/fma-$variant.o"
  emcc -O2 "build/w185/fma-$variant.o" -sEXPORTED_FUNCTIONS=_fma -sMODULARIZE -sEXPORT_NAME=createFma -sENVIRONMENT=web,node -o "build/w185/fma-$variant.js"
  node -e 'const fs=require("fs");console.log(process.argv[1],WebAssembly.validate(fs.readFileSync(process.argv[1])))' "build/w185/fma-$variant.wasm"
done
emcc -O2 -x c /dev/null -sEXPORTED_FUNCTIONS=_fma -sMODULARIZE -sEXPORT_NAME=createFma -sENVIRONMENT=web,node -o build/w185/fma-baseline.js
python3 tools/wasmfmaoracle.py build/w185/vectors.json
node tools/wasmfmacheck.mjs build/w185/vectors.json build/w185/fma-baseline.js build/w185/fma-original.js build/w185/fma-int128.js | tee build/w185/check.log

# Restore the baseline even when a link or numerical check fails.
mkdir -p build/w185/baseline-module
cp build/wasm/xppautx.js build/wasm/xppautx.wasm build/w185/baseline-module/
trap 'cp build/w185/baseline-module/xppautx.js build/w185/baseline-module/xppautx.wasm build/wasm/' EXIT
link_flags=$(sed -n 's/^WASM_LDFLAGS = //p' Makefile)
for variant in original int128; do
  mkdir -p "build/w185/$variant"
  rm build/wasm/xppautx.js
  make -s -j4 WASM=1 BUILDDIR=build/wasm CC=emcc CXX=em++ AR=emar OPT=-O2 "WASM_LDFLAGS=$link_flags build/w185/fma-$variant.o" build/wasm/xppautx.js
  cp build/wasm/xppautx.js build/wasm/xppautx.wasm build/wasm/client.mjs build/wasm/core.js "build/w185/$variant/"
  cp build/w185/baseline-module/xppautx.js build/w185/baseline-module/xppautx.wasm build/wasm/
  em++ -std=c++23 -O2 -Wall -Wextra -Werror -ffp-contract=off -Icore tools/wasmmathbench.cpp build/wasm/core_math_sin.o "build/w185/fma-$variant.o" -sENVIRONMENT=web,node -o "build/w185/math-$variant.js"
  node "build/w185/math-$variant.js" | tee "build/w185/math-$variant.log"
  em++ -std=c++23 -O2 -ffp-contract=off -pthread -Icore tests/test_math.cpp build/wasm/libxppcore.a "build/w185/fma-$variant.o" -sENVIRONMENT=node -sEXIT_RUNTIME=1 -o "build/w185/test-math-$variant.js"
  node "build/w185/test-math-$variant.js" | tee "build/w185/test-math-$variant.log"
  node tools/wasmcheck.mjs --bench --bench-total 10 --bench-rounds 5 --bench-series off --compare-wasm "build/w185/$variant" | tee "build/w185/model-$variant.log"
done
