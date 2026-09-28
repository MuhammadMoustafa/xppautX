/* W71 "a seed per run": xpp_next_seed (xpp_math.cpp) picks a following
   run's seed from the one just used, as a pure function of it -- the same
   run seed always picks the same next seed, whatever that run itself drew
   -- and the generator's full state (its mt19937_64 state plus normal()'s
   spare deviate) round-trips through xpp::xpp_rand_state_save/_load: after
   a save and a load elsewhere, the next draws are exactly what they would
   have been without saving at all. */
#include "xpptest.h"
#include "xpp_math.h"

#include <cstring>

int main(void)
{
    /* xpp_next_seed(seed) is deterministic in seed alone */
    CHECK(xpp_next_seed(42) == xpp_next_seed(42));
    CHECK(xpp_next_seed(42) != xpp_next_seed(43));
    CHECK(xpp_next_seed(1) >= 0); /* usable as the next "@ seed=" */

    /* it does not depend on the shared generator's own state: drawing
       from that generator first must not change what it returns */
    {
        const int before = xpp_next_seed(918273);
        nsrand48(1);
        for (int i = 0; i < 25; i++) ndrand48();
        const int after = xpp_next_seed(918273);
        CHECK(before == after);
    }

    /* state round-trip: save after a few draws, keep drawing to get the
       "expected" continuation, then reload the saved state elsewhere and
       check the same continuation comes back */
    nsrand48(7);
    for (int i = 0; i < 10; i++) ndrand48();       /* warm the generator, and its spare deviate via normal() */
    (void)normal(0.0, 1.0);
    const std::string state = xpp::xpp_rand_state_save();

    double expect[20];
    for (int i = 0; i < 20; i++) expect[i] = (i % 2 == 0) ? ndrand48() : normal(0.0, 1.0);

    /* disturb the generator, then load the saved state back */
    nsrand48(999);
    for (int i = 0; i < 30; i++) ndrand48();
    CHECK(xpp::xpp_rand_state_load(state));

    double got[20];
    for (int i = 0; i < 20; i++) got[i] = (i % 2 == 0) ? ndrand48() : normal(0.0, 1.0);
    CHECK(std::memcmp(expect, got, sizeof(expect)) == 0);

    /* a state string that is not one this function wrote is rejected,
       leaving the generator as it was (loading the good state above,
       drawing once more, saving, then failing to load garbage must not
       change what comes next) */
    const std::string good_again = xpp::xpp_rand_state_save();
    CHECK(!xpp::xpp_rand_state_load("not a generator state"));
    const std::string unchanged = xpp::xpp_rand_state_save();
    CHECK(good_again == unchanged);

    TEST_REPORT("seed stream and generator state round-trip");
}
