# MIR in xppautX (W706, W711, W714)

MIR (Medium Internal Representation), by Vladimir Makarov, MIT licence ([LICENSE](LICENSE); the notice is kept
and ships with the binaries). Master commit `dc2c763f1c8bc746102ce87933d866335f9e6c06` (2026-10-23), from
https://github.com/vnmakarov/mir/archive/dc2c763f1c8bc746102ce87933d866335f9e6c06.tar.gz (sha256
`0a444556877860ef3b95ae415b6403e9e8f9e054cb0d2686985b28f7e4ccb980`).

A commit and not a release: the latest release, v1.0.0 (2024-05-27), overflows a 2-element array in `mir-gen.c`'s
`try_spilled_reg_mem` on a spilled `x = x * x`, which aborts level 2 on mu26 and ma23; upstream fixed it on 2026-08-18
(PR #468) and no release has the fix. Upstream issue #475, a related `op_nums` overflow, is still open.

Every file here is upstream's, unedited. Left out: tests, benchmarks, build files (CMakeLists, GNUmakefile), the
command-line drivers, llvm2mir, mir2c, mir-utils, the c2mir C front end (xppautX builds the IR itself) and the ppc64,
s390x and riscv64 back ends (xppautX runs on x86-64 and arm64 only); `mir.c` and `mir-gen.c` include their back end by
the compiler's target macros. This file is xppautX's addition.

What is built: `mir.c` and `mir-gen.c`, each one object of the core library (the Makefile), without warnings of
ours (`-w`), LTO or contraction, and with `-DNDEBUG` as upstream builds. The adapter is `core/xpp_mir.{h,cpp}`: it
takes the Generator's trees (`core/expr_ir.h`, written by `core/expr_native.cpp`), builds the module through MIR's
API and generates the code of every function when the model is compiled. MIR's interpreter, its lazy generation and
its text and binary readers are not used.

Executable memory is MIR's own `mmap` (`mir-code-alloc-default.c`): `PROT_EXEC` pages that are made writable only
while code is written (`mprotect`), on Windows `VirtualAlloc` and `VirtualProtect`, and on macOS arm64 `MAP_JIT`
mappings with `pthread_jit_write_protect_np` toggled around each write (and `sys_icache_invalidate`). A signed,
hardened-runtime macOS build must carry the `com.apple.security.cs.allow-jit` entitlement; xppautX's releases are
unsigned, which asks for none. Each compile reserves 128 MB of address space (no memory) on 64-bit non-Windows
systems, so every piece of code is within a direct branch of the others.
