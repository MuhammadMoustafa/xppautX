/* stocHast's "Liapunov" (adj2.cpp hrw_liapunov): a linear system x'=a*x has
   a known maximal Liapunov exponent, exactly a, since every perturbation
   grows or shrinks as e^(a t) regardless of its size or direction. This
   checks hrw_liapunov() against that known value within a small tolerance,
   after integrating tools/models/stoch_liap.ode (rk4, a=-0.7) the way
   xppautX -silent does (Initialconds/Go). */
#include "xpptest.h"
#include "session.h"
#include "storage.h"
#include "adj2.h"
#include "browse.h"
#include "graphics.h"
#include "integrate.h"
#include "numerics.h"
#include "xpp_batch.h"
#include "load_eqn.h"
#include "menudrive.h"

#include <cmath>

int main(void)
{
    char arg0[] = "test_stochast_liap", arg1[] = "tools/models/stoch_liap.ode";
    char *argv[] = {arg0, arg1, NULL};
    xpp_load_model(2, argv, 1);
    xpp_batch_start();
    run_the_commands(xpp::session(), M_IG); /* Initialconds/Go, as -silent's script runs it */

    CHECK(xpp::session().data_store.rows > 100);

    auto liap = hrw_liapunov(xpp::session().numerics.newt_err);
    CHECK(liap.has_value());
    CHECK(liap && std::fabs(*liap - (-0.7)) < 1e-3);

    TEST_REPORT("stochast liapunov exponent");
}
