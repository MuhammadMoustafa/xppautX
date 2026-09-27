/* A load builds a new xpp::Model and xpp::Session and keeps them only when
   it succeeds (session.h's xpp::Load, W47c): a model that does not parse
   leaves the ones before current and untouched, where the program used to
   exit, and a good load after it still works. */
#include "xpptest.h"
#include "session.h"
#include "model.h"
#include "xpp_batch.h"

#include <string>

int main(void)
{
    char arg0[] = "test_load";
    char good[] = "tools/models/stoch_liap.ode";
    char bad[] = "tools/models/malformed_unbalanced.ode";
    char *argv_good[] = {arg0, good, NULL};
    char *argv_bad[] = {arg0, bad, NULL};

    CHECK(xpp_load_model(2, argv_good, 1) == 1);
    xpp::Model *model = &xpp::model();
    xpp::Session *session = &xpp::session();
    const int node = model->node;
    const std::string file = model->this_file;
    session->numerics.tend = 123.0; /* a change the failed load must keep */

    CHECK(xpp_load_model(2, argv_bad, 1) == 0);
    CHECK(&xpp::model() == model);
    CHECK(&xpp::session() == session);
    CHECK(xpp::model().node == node);
    CHECK(xpp::model().this_file == file);
    CHECK(xpp::session().numerics.tend == 123.0);

    CHECK(xpp_load_model(2, argv_good, 1) == 1);
    CHECK(xpp::model().node == node);
    CHECK(xpp::model().this_file == file);
    CHECK(xpp::session().numerics.tend != 123.0);

    TEST_REPORT("load: build, then swap");
}
