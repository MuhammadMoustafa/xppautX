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

    /* why: f's formula (line 4) does not compile */
    std::optional<xpp::Diagnostic> d = xpp::load_model(2, argv_bad, 1);
    CHECK(d.has_value());
    if (d) {
        CHECK_STR(d->file.c_str(), bad);
        CHECK(d->line == 4);
        CHECK(d->col == 0);
        CHECK_STR(d->source.c_str(), "f(x)=sin(x");
        CHECK(d->cause.find("Function F messed up") != std::string::npos);
        CHECK(d->text().starts_with(std::string(bad) + ":4: "));
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
    CHECK(d.has_value());
    if (d) {
        CHECK_STR(d->file.c_str(), odex);
        CHECK(d->line == 2 || d->line == 3);
        CHECK(d->col > 0);
        CHECK(!d->cause.empty());
    }

    CHECK(xpp_load_model(2, argv_good, 1) == 1);
    CHECK(xpp::model().node == node);
    CHECK(xpp::model().this_file == file);
    CHECK(xpp::session().numerics.tend != 123.0);

    TEST_REPORT("load: build, then swap");
}
