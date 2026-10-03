# xppautX in WebAssembly (W9, a proof of concept)

The core compiled with Emscripten runs in a Web Worker and speaks the JSON
protocol ([protocol.md](protocol.md)) over `postMessage`. This is a proof
that it can, not a product: nothing here ships with a release, and CI has
no emsdk. The maintainer's report on whether to go further is
`build/W9-report.md` (written by the card, not committed).

## Build and check

emsdk lives under the home folder (no sudo): `git clone
https://github.com/emscripten-core/emsdk ~/emsdk`, `cd ~/emsdk &&
./emsdk install latest && ./emsdk activate latest`. Build from WSL's own
copy, never over `/mnt/c`:

    tools/wslrun.sh tools/wasmcheck.sh            # build, page, check
    tools/wslrun.sh tools/wasmcheck.sh --bench    # also kuramot100's time

`make wasm` (with `emcc` on the PATH) links `build/wasm/xppautx.js` and
`.wasm`; `node wasm/build.mjs` adds the page, the worker and the client
bundle beside them (it needs web2's `npm ci`, whose esbuild and TypeScript
it uses); `tools/wasmcheck.mjs` is the check. It reads data only: the
module's series for `examples/ode/lecar.ode` equals `output.dat` of
`xppautX lecar.ode --silent` and the native `--server`'s, bit for bit;
Abort stops a long run; core.js refuses a file name that is not a base name.

`tools/wasmpagecheck.mjs` (Git Bash, like web2check; copy `build/wasm` from
WSL's copy first) drives the page in headless Chrome: isolated, the worker
integrates lecar.ode (601 rows), Abort stops a long run; without the headers
the page says it is not isolated and starts nothing.

To see the page: `node wasm/serve.mjs`, then open the printed address.

## How it works

- `core/xpp_wasm.cpp` replaces `xpp_http.cpp`: `xpp::http::emit` hands each
  event line to the worker (`Module.xppEmit`), and `xpp::wasm::push` (an
  embind function, so no `extern "C"`) is the page's commands' way into
  `xpp::inbox`, the job the `--server` stdin reader does natively.
  `xpp::http::active()` is true, so `ui_json.cpp` starts no stdin reader.
- main() runs in a pthread (`-sPROXY_TO_PTHREAD`): the core blocks in
  `xpp::inbox::next` as it does natively, and the worker's own thread stays
  free to take messages. `abort` and `quit` are control lines: `push` runs
  the inbox's classifier on the worker's thread, which cancels the job
  through `xpp::job`'s atomics, exactly the HTTP reader thread's path.
  The wasm module's memory is a `SharedArrayBuffer`; that is the card's
  "cancel token as a SharedArrayBuffer flag", with no code of its own.
- `wasm/core.js` starts the module (shared by `wasm/worker.js` and node's
  check); `wasm/src/client.ts` is a `Transport` for web2's `AppState`
  reducer over lines, so the page's series, appends and windows are web2's
  code, not a second reading of the protocol; `wasm/src/page.ts` is a
  minimal page (plot, Integrate, long run, Abort).
- Files: the model and what the core writes live in `/work` of the module's
  file system. `persistent` mounts IDBFS there (kept after `save`); not yet
  exercised by a check.

## What cross-origin isolation is for

Shared memory needs `crossOriginIsolated`: the page is served with
`Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy:
require-corp` (`wasm/serve.mjs` does; `--no-isolation` leaves them out). Any
host of the page, a GitHub Pages site included, must send them (GitHub
Pages cannot: a service worker shim is the usual workaround). Without them
the page says so and does not start the core.

## Left out, and why

See the comment above `WASM_LDFLAGS` in the Makefile: the HTTP server and
the window (a browser has no sockets and is the window), the stdin reader
(replaced by `push`), starting a process (`XPPEDITOR`, the browser opener:
there are none). The rest of the core is unchanged.
