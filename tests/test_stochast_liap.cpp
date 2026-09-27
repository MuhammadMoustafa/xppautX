/* stocHast's "Liapunov" (adj2.cpp hrw_liapunov): a linear system x'=a*x has
   a known maximal Liapunov exponent, exactly a, since every perturbation
   grows or shrinks as e^(a t) regardless of its size or direction. This
   checks hrw_liapunov() against that known value within a small tolerance,
   after integrating tools/models/stoch_liap.ode (rk4, a=-0.7) the way
   xppautX -silent does. */
#include "xpptest.h"
#include "storage.h"
#include "adj2.h"
#include "browse.h"
#include "graphics.h"
#include "integrate.h"
#include "numerics.h"
#include "xpp_batch.h"
#include "load_eqn.h"

#include <cmath>

int main(void)
{
    char arg0[] = "test_stochast_liap", arg1[] = "tools/models/stoch_liap.ode";
    char *argv[] = {arg0, arg1, NULL};
    xpp_load_model(2, argv, 1);
    init_browser();
    init_all_graph();
    batch_integrate();

    CHECK(data_store.rows > 100);

    double liap = 0;
    int ok = hrw_liapunov(&liap, 1, NEWT_ERR);
    CHECK(ok == 1);
    CHECK(std::fabs(liap - (-0.7)) < 1e-3);

    TEST_REPORT("stochast liapunov exponent");
}
