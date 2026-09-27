#ifndef XPP_MEM_H
#define XPP_MEM_H

/* xpp_mem.cpp. Every core allocation is C++ now (std::vector, std::string,
   std::unique_ptr, ...): the raw allocator this header used to declare
   (xpp_malloc/xpp_calloc/xpp_realloc/xpp_strdup/xpp_free, the
   XPP_MEM_FAIL_AT test hook, XPP_MEM_INIT, xpp_mem_stats()) was retired
   at W48, once tabular.cpp -- its last caller -- moved to a std::vector.
   What is left is the one thing nothing else replaces: the loud, final
   exit a failed allocation always took.

   C++ code whose std::string or std::vector could not allocate (it caught
   std::bad_alloc: no exception may reach its C callers, CLAUDE.md "C and
   C++") ends the program the same way xpp_malloc used to: an ERROR
   "out of memory <what>", then exit(1).

   The core never calls the C library's malloc/calloc/realloc/strdup/free
   directly; the exceptions are memory that crosses a boundary with a
   library, which keeps that library's allocator (Xlib's own memory,
   XReadBitmapFileData's in main.c, goes back through XFree; dirname()
   (auto_nox.c, aniparse.c) returns a pointer into its argument; getcwd()
   (xpp_files_dir.cpp) fills the caller's buffer). A new exception
   (getline, scandir, realpath(p, NULL), asprintf ...) is listed here and
   in tools/alloccheck.sh, which fails any other direct call
   (tools/sourcecheck.sh runs it).

   pocketfft (third_party/pocketfft, compiled into xpp_math.cpp) allocates
   its scratch memory with the C library's malloc/free inside its own
   header and hands none of it out; alloccheck reads core/ only.

   make asan (build/asan, AddressSanitizer + UBSan) and tools/asancheck.sh
   check that nothing leaks. */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus)
[[noreturn]]
#elif defined(__GNUC__)
__attribute__((noreturn))
#endif
void xpp_out_of_memory(const char *what);

#ifdef __cplusplus
}
#endif

#endif
