# MIR in xppautX (W706 spike)

MIR (Medium Internal Representation) and its C front end c2mir, by Vladimir Makarov, MIT licence
([LICENSE](LICENSE)). Master commit `dc2c763f1c8bc746102ce87933d866335f9e6c06` (2026-10-23), from
https://github.com/vnmakarov/mir/archive/dc2c763f1c8bc746102ce87933d866335f9e6c06.tar.gz (sha256
`0a444556877860ef3b95ae415b6403e9e8f9e054cb0d2686985b28f7e4ccb980`).

A commit and not a release: the latest release, v1.0.0 (2024-05-27), overflows a 2-element array in `mir-gen.c`'s
`try_spilled_reg_mem` on a spilled `x = x * x`, which aborts level 2 on mu26 and ma23; upstream fixed it on 2026-08-18
(PR #468) and no release has the fix. Upstream issue #475, a related `op_nums` overflow, is still open.

Every file here is upstream's, unedited. Left out: tests, benchmarks, build files (CMakeLists, GNUmakefile), the
command-line drivers (mir-bin-*, c2mir-driver.c), llvm2mir, mir2c, mir-utils and the ppc64, s390x and riscv64
back ends (xppautX runs on x86-64 and arm64 only); `mir.c` and `mir-gen.c` include their back end by the compiler's
target macros, so those three would only be needed on a target we do not build. This file is xppautX's addition.

Built only by `make mirspike` (never by `xppautx` or `test`): `mir.c`, `mir-gen.c` and `c2mir/c2mir.c`, each one
object of its own, without warnings of ours (`-w`), LTO or contraction. The adapter is `tools/mir-spike/xpp_mir.{h,cpp}`,
the comparison `tools/mir-spike/mir_spike.cpp`, the fixtures `tests/mir-spike/`.
