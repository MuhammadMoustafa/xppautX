/* stocHast's "Liapunov" (adj2.cpp hrw_liapunov): a linear system x'=a*x has
   a known maximal Liapunov exponent, exactly a, since every perturbation
   grows or shrinks as e^(a t) regardless of its size or direction. This
   checks hrw_liapunov() against that known value within a small tolerance,
   after integrating tools/models/stoch_liap.odex (rk4, a=-0.7) the way
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
    char arg0[] = "test_stochast_liap", arg1[] = "tools/models/stoch_liap.odex";
    char *argv[] = {arg0, arg1, NULL};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    xpp::batch_start(xpp::client_session());
    run_the_commands(xpp::client_session(), M_IG); /* Initialconds/Go, as -silent's script runs it */

    CHECK(xpp::client_session().data_store.rows > 100);

    auto liap = xpp::hrw_liapunov(xpp::client_session(), xpp::client_session().numerics.newt_err);
    CHECK(liap.has_value());
    CHECK(liap && std::fabs(*liap - (-0.7)) < 1e-3);

    TEST_REPORT("stochast liapunov exponent");
}
