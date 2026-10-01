/* stocHast's "New seed" (markov.cpp nsrand48/ndrand48): the same seed must
   give the same draws twice, and a different seed must give a different
   sequence. This is the guard W32a (replacing the Numerical Recipes ran1
   generator with std::mt19937_64) must keep passing with no edit here:
   the test only checks reproducibility and non-determinism between seeds,
   never an exact value tied to the current generator. */
#include "xpptest.h"
#include "markov.h"

#include <cstring>

int main(void)
{
    double a[50], b[50], c[50];

    xpp::nsrand48(42);
    for (int i = 0; i < 50; i++) a[i] = xpp::ndrand48();

    xpp::nsrand48(42);
    for (int i = 0; i < 50; i++) b[i] = xpp::ndrand48();

    xpp::nsrand48(43);
    for (int i = 0; i < 50; i++) c[i] = xpp::ndrand48();

    CHECK(std::memcmp(a, b, sizeof(a)) == 0); /* same seed: same run */

    int ndiff = 0;
    for (int i = 0; i < 50; i++)
        if (a[i] != c[i]) ndiff++;
    CHECK(ndiff > 40); /* a different seed: a different run */

    /* every draw stays in ran1's documented [0,1) range regardless of the
       underlying generator */
    bool in_range = true;
    for (int i = 0; i < 50; i++)
        if (a[i] < 0.0 || a[i] >= 1.0) in_range = false;
    CHECK(in_range);

    TEST_REPORT("stochast seed reproducibility");
}
