/* xpp_mem: the allocator the whole core goes through. Its promises (no NULL,
   0 bytes is a real pointer, calloc zeroes, strdup(NULL) is NULL) are what
   let callers skip their checks, and the counters are what --debug reports
   at exit. A failure itself exits the program, so it is checked end to end
   instead: tools/verify.sh runs XPP_MEM_FAIL_AT and expects the ERROR line
   naming the call site. */
#include "xpptest.h"
#include "xpp_mem.h"
#include <stdlib.h>
#include <string.h>

/* 1 when n bytes from p are all 0 */
static int all_zero(const char *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (p[i] != 0) return 0;
    return 1;
}

/* memory comes zeroed (W21): malloc's, and what a realloc adds, even when
   the C library hands back a block it had before or grows one in place;
   XPP_MEM_INIT=0 (tools/valgrindcheck.sh) turns that off */
static void check_zeroed(void)
{
    const char *init = getenv("XPP_MEM_INIT");
    char *p;
    int k;
    if (init != NULL && strcmp(init, "0") == 0) return;
    for (k = 0; k < 20; k++) {
        p = xpp_malloc(64);
        CHECK(all_zero(p, 64));
        memset(p, 0xAB, 64);
        xpp_free(p);
    }
    p = xpp_malloc(24);
    CHECK(all_zero(p, 24));
    memset(p, 'x', 24);
    p = xpp_realloc(p, 40); /* in place, into the block's own slack */
    CHECK(p[23] == 'x' && all_zero(p + 24, 16));
    p = xpp_realloc(p, 5000); /* moved */
    CHECK(p[23] == 'x' && all_zero(p + 24, 5000 - 24));
    xpp_free(p);
}

int main(void)
{
    XppMemStats s0 = xpp_mem_stats(), s1;
    char *a, *b, *c;
    int *z, i, zeroed = 1;

    /* 0 bytes: two valid, distinct pointers */
    a = xpp_malloc(0);
    b = xpp_malloc(0);
    CHECK(a != NULL && b != NULL && a != b);
    xpp_free(a);
    xpp_free(b);

    /* calloc zeroes, including when one factor is 0 */
    z = xpp_calloc(100, sizeof(int));
    for (i = 0; i < 100; i++) zeroed &= z[i] == 0;
    CHECK(zeroed);
    xpp_free(z);
    z = xpp_calloc(0, sizeof(int));
    CHECK(z != NULL);
    xpp_free(z);

    /* strdup copies; NULL stays NULL */
    c = xpp_strdup("lecar");
    CHECK_STR(c, "lecar");
    CHECK(xpp_strdup(NULL) == NULL);

    /* realloc keeps the content; realloc(NULL) is an allocation */
    c = xpp_realloc(c, 1000);
    CHECK_STR(c, "lecar");
    xpp_free(c);
    c = xpp_realloc(NULL, 10);
    CHECK(c != NULL);
    xpp_free(c);
    xpp_free(NULL); /* a no-op, not counted */

    s1 = xpp_mem_stats();
    /* malloc x2, calloc x2, strdup, realloc(NULL): 6 allocations */
    CHECK(s1.allocs - s0.allocs == 6);
    CHECK(s1.reallocs - s0.reallocs == 1);
    CHECK(s1.frees - s0.frees == 6);
    CHECK(s1.bytes - s0.bytes >= 100 * sizeof(int) + 6 + 1000 + 10);
    /* everything above was freed */
    CHECK(s1.live_bytes == s0.live_bytes);

    check_zeroed();
    TEST_REPORT("memory");
}
