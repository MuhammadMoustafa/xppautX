# TinyCC

Tiny C Compiler, version 0.9.28rc, from the `mob` branch of
https://repo.or.cz/tinycc.git at commit `43c7708b` (2026-10-03).
The source here is `git archive 43c7708b`; no upstream file was edited.
Only upstream's `.gitignore` was removed: its `conftest*` and `tcc` rules
hid `conftest.c` (needed to build c2str) and `win32/include/tcc/tcc_libm.h`
from the initial import. Both are retained verbatim. No code, tests,
examples or documentation were removed. This README is xppautX's addition.
License: LGPL 2.1,
[COPYING](COPYING). Integration: W254, [#311](https://github.com/MuhammadMoustafa/xppautX/issues/311).

## Build and ownership

The root Makefile builds `libtcc.c` with `ONE_SOURCE=1` into `libtcc.o` in
`libxppcore.a`. No installed TinyCC, subprocess compiler, libtcc DLL or
runtime support archive is needed. This card supplies the adapter and tests;
model compilation is W255. Only `core/xpp_tcc.cpp` includes `libtcc.h`.
It uses the explicit relative path: adding this directory to the C++ include
search would let `VERSION` shadow libc++'s `<version>` on Windows.

`tools/tinycc/config.h` is our configure-equivalent header, outside the
verbatim source. The build compiler's target macros select `TCC_TARGET_X86_64`
or `TCC_TARGET_ARM64`, plus PE on Windows, Mach-O on macOS, or ELF on Linux.
Other targets are a build error. `TCC_VERSION` is `0.9.28rc`.

The configuration defines `CONFIG_TCC_STATIC=1` (disable dynamic loading),
`CONFIG_TCCBOOT=1` (omit TinyCC's built-in libc symbol table), and empty
`CONFIG_TCCDIR`, `CONFIG_TCC_SYSINCLUDEPATHS`, `CONFIG_TCC_LIBPATHS` and
`CONFIG_TCC_CRTPREFIX`. `CONFIG_TCC_BCHECK=0` and `CONFIG_TCC_BACKTRACE=0`
disable instrumentation needing runtime objects. `CONFIG_TCC_PREDEFS=1`
embeds predefined declarations. `CONFIG_RUNMEM_RO=1` selects write-then-execute
pages on all platforms. `CONFIG_RUNMEM_VIRTUALALLOC=0` selects the existing
heap allocation path (reason below). The normal upstream semaphore support
remains enabled.

The Makefile builds upstream `conftest.c` with `-DC2STR` into
`$(BUILDDIR)/tinycc/c2str`, then runs it on `include/tccdefs.h` to generate
`$(BUILDDIR)/tinycc/tccdefs_.h`; Windows adds `.exe`. Generated files are
not committed. Include dependencies are tracked for the one-source object.
Vendored C uses GNU C99, the build's optimization/sanitizer flags without LTO,
`-ffp-contract=off`, and `-w`, as miniz does. Our C++ retains its normal
warning flags, including `WERROR=1`.

The adapter passes `-nostdlib -nostdinc -Wl,-nostdlib` before selecting memory
output, and supplies external addresses with `tcc_add_symbol`. Standard
headers and runtime libraries are not loaded. Upstream still records native
Windows system/macOS SDK library directories during output setup; with these
options and the static loader they are unused by this compilation path.
`Program::compile` returns only after successful relocation, so callers cannot
relocate twice (which TinyCC treats as fatal). Errors retain TinyCC's diagnostic
and the caller's `xpp::Place`. Missing functions are errors too.

TinyCC owns its compiler allocations and relocated memory. `tcc_delete`
frees them with the state; a returned function address is valid only while
its `Program` lives. Diagnostics are copied through `tcc_set_error_func`;
allocation failure in that C callback uses `xpp::out_of_memory`.

## Executable memory

Generated code needs memory that is first written, then executed. In this
revision Linux uses `tcc_malloc` plus page alignment and `mprotect`, not
anonymous `mmap`. Upstream's optional `CONFIG_SELINUX` uses two file-backed
`mmap` views; we do not enable it. Windows normally uses
`VirtualAlloc(PAGE_READWRITE)` then `VirtualProtect`, but this revision's
`tcc_run_free` calls `VirtualFree(ptr, size, MEM_RELEASE)` with nonzero size.
A native Windows probe returned error 87; `MEM_RELEASE` requires size zero.
We select upstream's `CONFIG_RUNMEM_VIRTUALALLOC=0` path: `tcc_malloc` with
page alignment, `VirtualProtect` for execution, then restore writable pages
and `tcc_free` in `tcc_delete`. No vendored file or allocator shim is needed.

TinyCC's own Apple path sets `CONFIG_RUNMEM_RO=1` on `__APPLE__`, so pages
are written, then `mprotect`'ed read-only/read-exec, never writable and
executable at once. Apple silicon allows that RW->RX flip for a process
without the hardened runtime; with the hardened runtime it needs the
entitlement `com.apple.security.cs.allow-unsigned-executable-memory`
(not `allow-jit`, since TinyCC uses no `MAP_JIT`). It does not use
`pthread_jit_write_protect_np`. macOS is untested here: CI's macos jobs and
the reviewer cover it. Linux builds and checks are also the reviewer's.

If `tcc_relocate` fails on any platform (for example `mprotect` or
`VirtualProtect` is refused), the adapter returns
`xpp::Error` with TinyCC's message. W255 shows it and runs the interpreter,
per Discussion #309 decision 2; there is no model execution change here.

## Trust boundary

This API accepts trusted generated C only. `-nostdinc` and `-nostdlib` remove
dependencies; they are not a sandbox for hostile C. Explicit includes can
still read files, C can contain arbitrary operations, and compiler resource
exhaustion is possible (upstream allocation failure can terminate the process).
W255 must generate source from validated expressions, never pass user C
through. This card exposes no command or file reader to the adapter.
