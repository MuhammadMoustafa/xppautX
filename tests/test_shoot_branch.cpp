/* W533: an invariant set's branch is stored whole from the saddle, whatever store_from says:
   shoot_easy runs the unstable branch of x' = x, y' = -y (store_from 0.5, dt 0.05, length 1, 20 steps) and the
   data holds every step's row, from the saddle's time, not only those past store_from. */
#include "local_fs.h"
#include "xpptest.h"
#include "session.h"
#include "numerics_settings.h"
#include "model_load.h"
#include "comline.h"
#include "integrate.h"
#include "storage.h"
#include <utility>
#include <variant>
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

TEST_CASE("test_shoot_branch")
{
    auto line = xpp::parse_command_line({"test_shoot_branch", "tools/models/saddle_manifolds_transient.odex", "--convert"});
    CHECK(line.has_value());
    if (!line) return 1;
    auto loaded = xpp::load_model(local_fs(), std::move(*line), 1);
    CHECK(loaded.has_value());
    if (!loaded) return 1;
    xpp::Session &s = **loaded;
    s.numerics->storflag = true;
    s.numerics->delta_t = .05;
    s.numerics->total_time = 1;
    CHECK(s.numerics->store_from == .5);

    double x[2] = {1e-4, 0};
    CHECK(xpp::shoot_easy(s, x).has_value());
    /* 20 steps of the branch from the start: the whole branch is stored, not only the 10 past store_from */
    CHECK(s.data_store->rows >= 20);
    CHECK(s.data_store->col[0][0] < s.numerics->store_from);
    TEST_REPORT("shoot branch");
}
