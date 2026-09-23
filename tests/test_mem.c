/* xpp_mem: the allocator the whole core goes through. Its promises (no NULL,
   0 bytes is a real pointer, calloc zeroes, strdup(NULL) is NULL) are what
   let callers skip their checks, and the counters are what --debug reports
   at exit. A failure itself exits the program, so it is checked end to end
   instead: tools/verify.sh runs XPP_MEM_FAIL_AT and expects the ERROR line
   naming the call site. */
#include "xpptest.h"
#include "xpp_mem.h"

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

    TEST_REPORT("memory");
}
