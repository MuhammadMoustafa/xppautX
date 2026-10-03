/* The WebAssembly build's front door (W9, a proof of concept; built only
   by `make wasm`, never by the native targets).

   xppautX built with Emscripten runs the --server session inside a Web
   Worker (wasm/worker.js): the page's commands arrive as postMessage
   events, the worker hands each to xpp::wasm::push, and the core's events
   go back through xpp::http::emit to the worker, which posts them to the
   page. Both halves stand in for what the native program does with other
   files: this one replaces xpp_http.cpp (no sockets in a browser) and
   the --server stdin reader (xpp_inbox.cpp's start_stdin, which ui_json.cpp
   skips while xpp::http::active() is true).

   Threads: Emscripten's pthreads run on SharedArrayBuffer memory. The
   build proxies main() to a pthread (-sPROXY_TO_PTHREAD), so the core
   keeps its own thread, blocking in xpp::inbox::next as it does natively,
   and the worker's thread stays free to take messages. Abort and Quit are
   control lines: push() runs the inbox's classifier on the worker's
   thread, which cancels the job through xpp::job's atomics in the shared
   memory, the same path as the HTTP reader thread natively. That shared
   memory is the "SharedArrayBuffer flag" of the card, and it is why the
   page must be cross-origin isolated (docs/wasm.md). */
#include "xpp_http.h"
#include "xpp_inbox.h"
#include <emscripten.h>
#include <emscripten/bind.h>
#include <string>

namespace xpp::http {

/* No server: the page is the worker's client. main() reaches these only in
   the modes that serve a page, which this build never selects (it runs
   --server). */
bool start(int, bool, bool) { return false; }
void show(bool) {}

/* the protocol goes to the worker's client, never to stdin/stdout */
bool active() { return true; }

/* One event to the worker, which owns the page's port (Module.xppEmit,
   wasm/worker.js). The core's thread is a pthread, so the JS runs on the
   worker's own thread (MAIN_THREAD_EM_ASM waits for it): events arrive in
   the order they were emitted, and the line is read before it is freed. */
void emit(std::string_view line)
{
/* EM_ASM's arguments are named $0, $1 (a clang extension that -pedantic reports) */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
    MAIN_THREAD_EM_ASM({ Module.xppEmit(UTF8ToString($0, $1)); }, line.data(), line.size());
#pragma clang diagnostic pop
}

} // namespace xpp::http

namespace xpp::wasm {
namespace {

/* One command line from the page: the --server stdin reader's job. */
void push(const std::string &line) { xpp::inbox::push(line); }

EMSCRIPTEN_BINDINGS(xppautx)
{
    emscripten::function("push", &push);
}

} // namespace
} // namespace xpp::wasm
