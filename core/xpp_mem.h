#ifndef XPP_MEM_H
#define XPP_MEM_H

/* The core's allocator (xpp_mem.cpp).

   xpp_malloc, xpp_calloc, xpp_realloc and xpp_strdup never return NULL:
   when the C library cannot give the memory they log an ERROR naming the
   size and the call site (file:line, which the macros below pass) and
   exit(1). So a caller does not check the result. A request of 0 bytes
   gets a valid, distinct pointer; xpp_calloc(n, size) whose n*size does
   not fit a size_t is a failure too. xpp_strdup(NULL) is NULL. xpp_free
   is free(): it takes NULL and any pointer these functions returned.

   Memory comes zeroed: xpp_malloc's like xpp_calloc's, and the part a
   xpp_realloc adds (W21). A read of memory never written then reads 0 on
   every platform, where it read whatever the heap held, which differed
   between runs and systems. XPP_MEM_INIT=0 turns that off, for valgrind
   to report such a read (tools/valgrindcheck.sh sets it): a bug still,
   fixed where valgrind finds it.

   Everything is counted (xpp_mem_stats()); the counts are logged at exit
   at XPP_LOG_DEBUG (--debug). The counters are atomics: the protocol's
   reader threads allocate too.

   Test hook: the environment variable XPP_MEM_FAIL_AT=N makes the N-th
   allocating call (malloc, calloc, realloc or strdup, counted together)
   fail, to show that a failure is loud and names its site:
       XPP_MEM_FAIL_AT=5 ./xppautX examples/ode/lecar.ode -silent
   Its companion for the Linux window, XPP_WINDOW_FAIL_LOAD=1, makes the
   window's library fail to load as if WebKitGTK were missing
   (xpp_window_loader.cpp; tools/modecheck.sh).

   Every core file allocates through these. The exceptions are memory
   that crosses a boundary with a library, which keeps that library's
   allocator, because xpp_free must only see what xpp_* allocated (and
   memory we allocate must not be freed by a library):
   - Xlib's own memory (XReadBitmapFileData's in main.c) goes back
     through XFree, as before.
   That is all today: dirname() (auto_nox.c, aniparse.c) returns a pointer
   into its argument, getcwd() (read_dir.c) fills the caller's buffer, and
   no core file frees what the C library allocated. A new exception
   (getline, scandir, realpath(p, NULL), asprintf ...) stays on the C
   library's malloc/free and is listed here and in tools/alloccheck.sh,
   which fails any other direct call (tools/sourcecheck.sh runs it).

   CVODE (cv*.c, dense.c, band.c, llnlmath.c,
   vector.c) and AUTO (autlib*.c, setubv2.c, ...) are ours: they allocate
   and free their own memory and hand none of it to anyone else, so they
   use these functions like the rest.

   make asan (build/asan, AddressSanitizer + UBSan) and tools/asancheck.sh
   check that nothing leaks. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *xpp_malloc_at(size_t n, const char *file, int line);
void *xpp_calloc_at(size_t n, size_t size, const char *file, int line);
void *xpp_realloc_at(void *p, size_t n, const char *file, int line);
char *xpp_strdup_at(const char *s, const char *file, int line);
void xpp_free(void *p);

typedef struct {
    unsigned long long allocs;   /* malloc, calloc, strdup, realloc(NULL, n) */
    unsigned long long reallocs; /* realloc of a live pointer */
    unsigned long long frees;    /* xpp_free of a non-NULL pointer */
    unsigned long long bytes;    /* bytes requested, all calls together */
    long long live_bytes;        /* held now, as the C library counts them
                                    (usable size); 0 where it cannot tell */
} XppMemStats;

XppMemStats xpp_mem_stats(void);

#ifdef __cplusplus
}
#endif

#define xpp_malloc(n) xpp_malloc_at((n), __FILE__, __LINE__)
#define xpp_calloc(n, size) xpp_calloc_at((n), (size), __FILE__, __LINE__)
#define xpp_realloc(p, n) xpp_realloc_at((p), (n), __FILE__, __LINE__)
#define xpp_strdup(s) xpp_strdup_at((s), __FILE__, __LINE__)

#endif
