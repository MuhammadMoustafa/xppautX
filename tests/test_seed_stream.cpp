/* W71 "a seed per run": xpp::next_seed (xpp_math.cpp) picks a following
   run's seed from the one just used, as a pure function of it -- the same
   run seed always picks the same next seed, whatever that run itself drew
   -- and a generator's full state (its mt19937_64 state plus normal()'s
   spare deviate) round-trips through xpp::Random's save and load: after
   a save and a load elsewhere, the next draws are exactly what they would
   have been without saving at all. */
#include "xpptest.h"
#include "xpp_math.h"
#include "random_state.h"

#include <cstring>
#include <random>

int main(void)
{
    /* A Windows literal must load identically with libc++ and libstdc++;
       a round-trip alone cannot detect implementation-defined streams. */
    xpp::Random portable;
    CHECK(portable.load(WINDOWS_STATE));
    CHECK(portable.save() == WINDOWS_STATE);
    CHECK(portable.normal(0.0, 1.0) == 0.125);
    for (double expected : WINDOWS_NEXT) CHECK(portable.uniform() == expected);
    std::mt19937_64 reference(7);
    portable.seed(7);
    constexpr int CROSS_TWISTS = 1000; /* crosses three MT state boundaries */
    for (int i = 0; i < CROSS_TWISTS; ++i)
        CHECK(portable.uniform() == (static_cast<double>(reference() >> 11) + 0.5) * 0x1.0p-53);
    const std::string untouched = portable.save();
    CHECK(!portable.load(std::string(WINDOWS_STATE) + " extra"));
    std::string foreign_layout = WINDOWS_STATE;
    foreign_layout.erase(foreign_layout.rfind(" 10 1 0.125"), 3); /* libc++ has no array index */
    CHECK(!portable.load(foreign_layout));
    CHECK(!portable.load("-1 " + std::string(WINDOWS_STATE)));
    CHECK(portable.save() == untouched);

    /* xpp::next_seed(seed) is deterministic in seed alone */
    CHECK(xpp::next_seed(42) == xpp::next_seed(42));
    CHECK(xpp::next_seed(42) != xpp::next_seed(43));
    CHECK(xpp::next_seed(1) >= 0); /* usable as the next "@ seed=" */

    /* it does not depend on a generator's own state: drawing from one
       first must not change what it returns */
    xpp::Random r;
    {
        const int before = xpp::next_seed(918273);
        r.seed(1);
        for (int i = 0; i < 25; i++) r.uniform();
        const int after = xpp::next_seed(918273);
        CHECK(before == after);
    }

    /* state round-trip: save after a few draws, keep drawing to get the
       "expected" continuation, then reload the saved state elsewhere and
       check the same continuation comes back */
    r.seed(7);
    for (int i = 0; i < 10; i++) r.uniform();       /* warm the generator, and its spare deviate via normal() */
    (void)r.normal(0.0, 1.0);
    const std::string state = r.save();

    double expect[20];
    for (int i = 0; i < 20; i++) expect[i] = (i % 2 == 0) ? r.uniform() : r.normal(0.0, 1.0);

    /* disturb the generator, then load the saved state back */
    r.seed(999);
    for (int i = 0; i < 30; i++) r.uniform();
    CHECK(r.load(state));

    double got[20];
    for (int i = 0; i < 20; i++) got[i] = (i % 2 == 0) ? r.uniform() : r.normal(0.0, 1.0);
    CHECK(std::memcmp(expect, got, sizeof(expect)) == 0);

    /* and in another generator (another Session's) the same */
    xpp::Random other;
    CHECK(other.load(state));
    for (int i = 0; i < 20; i++) got[i] = (i % 2 == 0) ? other.uniform() : other.normal(0.0, 1.0);
    CHECK(std::memcmp(expect, got, sizeof(expect)) == 0);

    /* a state string that is not one this function wrote is rejected,
       leaving the generator as it was (loading the good state above,
       drawing once more, saving, then failing to load garbage must not
       change what comes next) */
    const std::string good_again = r.save();
    CHECK(!r.load("not a generator state"));
    const std::string unchanged = r.save();
    CHECK(good_again == unchanged);

    TEST_REPORT("seed stream and generator state round-trip");
}
