/* stocHast's "Liapunov" (adj2.cpp hrw_liapunov): a linear system x'=a*x has
   a known maximal Liapunov exponent, exactly a, since every perturbation
   grows or shrinks as e^(a t) regardless of its size or direction. This
   checks hrw_liapunov() against that known value within a small tolerance,
   after integrating tools/models/stoch_liap.odex (rk4, a=-0.7) the way
   xppautX --silent does (Initialconds/Go). */
#include "load_args.h"
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
#include "command_registry.h"
#include "xpp_ui.h"

#include <cmath>

TEST_CASE("test_stochast_liap")
{
    char arg0[] = "test_stochast_liap", arg1[] = "tools/models/stoch_liap.odex";
    char *argv[] = {arg0, arg1, NULL};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::batch_start(xpp::client_session());
    CHECK(xpp::command::run_button(xpp::client_session(), "{\"cmd\":\"run.initial\"}", xpp::command_place())); /* Initialconds/Go, as --silent's script runs it */

    CHECK(xpp::client_session().data_store->rows > 100);

    auto liap = xpp::hrw_liapunov(xpp::client_session(), xpp::client_session().numerics->equilibria.jacobian_step);
    CHECK(liap.has_value());
    CHECK(liap && std::fabs(*liap - (-0.7)) < 1e-3);

    TEST_REPORT("stochast liapunov exponent");
}
