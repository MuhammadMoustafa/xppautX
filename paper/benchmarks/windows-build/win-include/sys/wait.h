/* W244: MinGW has no sys/wait.h. menudrive.c's XPPEDITOR and help-browser
   commands fork; the benchmark build never reaches them (-silent), and
   x11-stubs.c's fork() aborts if it does. */
#ifndef XPP_BENCH_SYS_WAIT_H
#define XPP_BENCH_SYS_WAIT_H
#include <sys/types.h>
int fork(void);
int wait(int *status);
#endif
