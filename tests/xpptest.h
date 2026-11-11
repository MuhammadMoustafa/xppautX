/* The whole test framework: a counter and a few macros. Each test file is one
   named test of the single program tests/test_main.cpp: TEST_CASE("test_x")
   { ... } ending with TEST_REPORT, and `make test` runs them all in one
   process (names on the command line run some, --list lists them, --reverse
   runs them from the last: no test depends on another, tests/README.md).

   These are unit tests over libxppcore, for pure code whose breakage an
   end-to-end run would report as a puzzling difference somewhere else. The
   behaviour of whole sessions is covered by tools/servercheck.py,
   tools/webcheck.py and tools/examples_check.sh. */
#ifndef XPPTEST_H
#define XPPTEST_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

[[maybe_unused]] static int tests_run, tests_failed; /* per test file: its own counts */

/* stdout unbuffered: a test that crashes keeps every FAIL line it printed
   before, which a pipe's buffer (CI's log) would otherwise lose with it */
static const int tests_stdout_unbuffered = setvbuf(stdout, NULL, _IONBF, 0);

/* an environment variable set to value, or removed (value null), on every platform: 0 when it worked */
static inline int test_setenv(const char *name, const char *value)
{
#ifdef _WIN32
    return _putenv_s(name, value ? value : "");
#else
    return value ? setenv(name, value, 1) : unsetenv(name);
#endif
}

/* the registry of tests/test_main.cpp */
struct TestRegistration {
    TestRegistration(const char *name, int (*run)());
};

/* a test: its name (its file's), then its body, which returns 0 when it passed (TEST_REPORT) */
#define TEST_CASE(name)                                                   \
    static int test_case_run();                                           \
    static const TestRegistration test_case_registration(name, test_case_run); \
    static int test_case_run()

/* the file that reads as empty and takes what is written to it */
static inline const char *test_null_device()
{
#ifdef _WIN32
    return "NUL";
#else
    return "/dev/null";
#endif
}

#define CHECK(cond) do {                                                \
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
