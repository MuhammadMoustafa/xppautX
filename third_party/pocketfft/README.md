# pocketfft

`pocketfft_hdronly.h` of Martin Reinecke's pocketfft, the C++ header-only
version (https://github.com/mreineck/pocketfft, branch `cpp`, commit
c90e55b3d529f8efa40ed01a20de22405f45fc65 of 2026-06-30), unchanged.
License: LICENSE.md (BSD-3-Clause).

A fast Fourier transform of any length. xppautX includes it only from
core/xpp_math.cpp (docs/roadmap.md W32a), which compiles it without its
thread pool (`POCKETFFT_NO_MULTITHREADING`: the core is single-threaded)
and gives the rest of the core a small API of its own, so no other file
sees pocketfft. It allocates its scratch memory with the C library's
malloc/free and hands none of it out (core/xpp_mem.h).
