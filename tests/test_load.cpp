/* A load builds a new xpp::Model and xpp::Session and keeps them only when
   it succeeds (session.h's xpp::Load, W47c): a model that does not parse
   leaves the ones before current and untouched, where the program used to
   exit, and a good load after it still works. A load that fails says why
   and where (xpp::load_model's xpp::Diagnostic, W63c). */
#include "xpptest.h"
#include "session.h"
#include "model.h"
#include "xpp_batch.h"
#include "xpp_io.h"

#include <optional>
#include <string>

int main(void)
{
    char arg0[] = "test_load";
    char good[] = "tools/models/stoch_liap.ode";
    char bad[] = "tools/models/malformed_unbalanced.ode";
    char *argv_good[] = {arg0, good, NULL};
    char *argv_bad[] = {arg0, bad, NULL};

    CHECK(xpp::load_model(2, argv_good, 1).has_value());
    xpp::Model *model = &xpp::client_session().model();
    xpp::Session *session = &xpp::client_session();
    const int node = model->node;
    const std::string file = model->this_file;
    session->numerics.tend = 123.0; /* a change the failed load must keep */

    CHECK(!xpp::load_model(2, argv_bad, 1).has_value());
    CHECK(&xpp::client_session().model() == model);
    CHECK(&xpp::client_session() == session);
    CHECK(xpp::client_session().model().node == node);
    CHECK(xpp::client_session().model().this_file == file);
    CHECK(xpp::client_session().numerics.tend == 123.0);

    /* why: f's formula (line 4) does not compile */
    xpp::Loaded d = xpp::load_model(2, argv_bad, 1);
    CHECK(!d.has_value());
    if (!d) {
        CHECK_STR(d.error().file.c_str(), bad);
        CHECK(d.error().line == 4);
        CHECK(d.error().col == 0);
        CHECK_STR(d.error().source.c_str(), "f(x)=sin(x");
        CHECK(d.error().cause.find("Function F messed up") != std::string::npos);
        CHECK(d.error().text().starts_with(std::string(bad) + ":4: "));
    }

    /* an .odex problem keeps its line and column */
    const char odex[] = "build/test_load_bad.odex";
    {
        xpp::Writer w(odex);
        CHECK(w && w.write("par a = 1\nx' = -x +\n") && w.commit());
    }
    char odex_arg[] = "build/test_load_bad.odex";
    char *argv_odex[] = {arg0, odex_arg, NULL};
    d = xpp::load_model(2, argv_odex, 1);
    CHECK(!d.has_value());
    if (!d) {
        CHECK_STR(d.error().file.c_str(), odex);
        CHECK(d.error().line == 2 || d.error().line == 3);
        CHECK(d.error().col > 0);
        CHECK(!d.error().cause.empty());
    }

    CHECK(xpp::load_model(2, argv_good, 1).has_value());
    CHECK(xpp::client_session().model().node == node);
    CHECK(xpp::client_session().model().this_file == file);
    CHECK(xpp::client_session().numerics.tend != 123.0);

    TEST_REPORT("load: build, then swap");
}
