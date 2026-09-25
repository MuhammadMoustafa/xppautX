#include "xpp_io.h" /* first: C++ headers before auto_f2c.h's min/max macros */
#include "auto_f2c.h"
#include "auto_c.h"
#include "auto_types.h"

#ifdef TIME
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
static double time_start (void) {
  struct rusage time;
  double seconds,microseconds;
  getrusage(RUSAGE_SELF,&time);
  seconds = (double)time.ru_utime.tv_sec;
  microseconds = (double)time.ru_utime.tv_usec;
  return seconds + microseconds/1e6;
}
static double time_end(double start) {
  struct rusage time;
  double seconds,microseconds;
  getrusage(RUSAGE_SELF,&time);
  seconds = (double)time.ru_utime.tv_sec;
  microseconds = (double)time.ru_utime.tv_usec;
  return (seconds + microseconds/1e6)-start;
}
#endif

/* The memory for these are taken care of in autobv_, autoae_, and setubv for the
   mpi parallel case */
/* global_scratch: auto_c.h */

/* The memory for these are taken care of in autobv_ and autoae_ */
/* global_rotations: auto_c.h */





