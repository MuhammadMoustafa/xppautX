# pocketfft

`pocketfft_hdronly.h` of Martin Reinecke's pocketfft, the C++ header-only
version (https://github.com/mreineck/pocketfft, branch `cpp`, commit
c90e55b3d529f8efa40ed01a20de22405f45fc65 of 2026-06-30), with one change (below).
License: LICENSE.md (BSD-3-Clause).

A fast Fourier transform of any length. xppautX includes it only from
core/xpp_math.cpp (docs/roadmap.md W32a), which compiles it without its
thread pool (`POCKETFFT_NO_MULTITHREADING`: the core is single-threaded)
and gives the rest of the core a small API of its own, so no other file
sees pocketfft. It allocates its scratch memory with the C library's
malloc/free and hands none of it out (core/xpp_mem.h).

The one change (W159): the twiddle factors' `std::cos` and `std::sin` calls (the
`sincos_2pibyn` class) are the macros `POCKETFFT_COS` and `POCKETFFT_SIN`,
which default to the same `std::` calls. core/xpp_math.cpp defines them as
its correctly rounded `xpp::math::cos`/`sin` before the include, so a Fourier
transform's twiddles, and its results, are the same on every CPU and system.
