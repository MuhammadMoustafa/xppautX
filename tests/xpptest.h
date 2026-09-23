/* The whole test framework: a counter and two macros. Each test file has a
   main() that ends with TEST_REPORT, and `make test` runs them all.

   These are unit tests over libxppcore, for pure code whose breakage an
   end-to-end run would report as a puzzling difference somewhere else. The
   behaviour of whole sessions is covered by tools/servercheck.py,
   tools/webcheck.py and tools/examples_check.sh. */
#ifndef XPPTEST_H
#define XPPTEST_H
#include <stdio.h>
#include <string.h>

static int tests_run, tests_failed;

#define CHECK(cond) do {                                                  \
    tests_run++;                                                          \
    if (!(cond)) {                                                        \
        tests_failed++;                                                   \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                     \
} while (0)

#define CHECK_STR(got, want) do {                                         \
    tests_run++;                                                          \
    if (strcmp((got), (want)) != 0) {                                     \
        tests_failed++;                                                   \
        printf("FAIL %s:%d\n  got  \"%s\"\n  want \"%s\"\n",              \
               __FILE__, __LINE__, (got), (want));                        \
    }                                                                     \
} while (0)

#define TEST_REPORT(name) do {                                            \
    printf("%-22s %d checks, %d failed\n", (name), tests_run, tests_failed); \
    return tests_failed ? 1 : 0;                                          \
} while (0)

#endif
