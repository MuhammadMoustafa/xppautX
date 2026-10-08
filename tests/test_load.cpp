/* A load builds a new xpp::Model and xpp::Session and keeps them only when
   it succeeds (session.h's xpp::Load, W47c): a model that does not parse
   leaves the ones before current and untouched, where the program used to
   exit, and a good load after it still works. A load that fails says why
   and where (xpp::load_model's xpp::Error, W63c). */
#include "xpptest.h"
#include "session.h"
#include "model.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "xpp_files.h"

#include <optional>
#include <string>

int main(void)
{
    char arg0[] = "test_load";
    char good[] = "tools/models/stoch_liap.odex";
    char bad[] = "tools/models/malformed_unbalanced.ode";
    char *argv_good[] = {arg0, good, NULL};
    char *argv_bad[] = {arg0, bad, NULL};

    /* an error's one rendering (xpp_error.h, W140): file:line:col: what,
       leaving out what is not known */
    CHECK(xpp::Error({"w", "bad", {"m.ode", 3, 7}}).text() == "m.ode:3:7: bad");
    CHECK(xpp::Error({"w", "bad", {"m.ode", 3}}).text() == "m.ode:3: bad");
    CHECK(xpp::Error({"w", "bad", {"m.ode"}}).text() == "m.ode: bad");
    CHECK(xpp::Error({"w", "bad", {"", 5}}).text() == "line 5: bad");
    CHECK(xpp::Error({"w", "bad"}).text() == "bad");

    CHECK(xpp::load_model(2, argv_good, 1).has_value());
    xpp::Model *model = &xpp::client_session().model();
    xpp::Session *session = &xpp::client_session();
    const int node = model->node;
    const std::string file = model->this_file;
    session->numerics.total_time = 123.0; /* a change the failed load must keep */

    CHECK(!xpp::load_model(2, argv_bad, 1).has_value());
    CHECK(&xpp::client_session().model() == model);
    CHECK(&xpp::client_session() == session);
    CHECK(xpp::client_session().model().node == node);
    CHECK(xpp::client_session().model().this_file == file);
    CHECK(xpp::client_session().numerics.total_time == 123.0);

    /* why: f's formula (line 4) does not compile */
    xpp::Loaded d = xpp::load_model(2, argv_bad, 1);
    CHECK(!d.has_value());
    if (!d) {
        CHECK_STR(d.error().place.file.c_str(), bad);
        CHECK(d.error().place.line == 4);
        CHECK(d.error().place.col == 0);
        CHECK_STR(d.error().place.source.c_str(), "f(x)=sin(x");
        CHECK(d.error().what.find("Function F messed up") != std::string::npos);
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
        CHECK_STR(d.error().place.file.c_str(), odex);
        CHECK(d.error().place.line == 2 || d.error().place.line == 3);
        CHECK(d.error().place.col > 0);
        CHECK(!d.error().what.empty());
    }

    CHECK(xpp::load_model(2, argv_good, 1).has_value());
    CHECK(xpp::client_session().model().node == node);
    CHECK(xpp::client_session().model().this_file == file);
    CHECK(xpp::client_session().numerics.total_time != 123.0);

    /* a model's @ colormap is its own Session's (W120): the next model
       loaded without one gets the default scale */
    const char cmap[] = "build/test_load_cmap.ode";
    {
        xpp::Writer w(cmap);
        CHECK(w && w.write("x'=-x\n@ colormap=3\ndone\n") && w.commit());
    }
    char cmap_arg[] = "build/test_load_cmap.ode";
    char *argv_cmap[] = {arg0, cmap_arg, NULL};
    CHECK(xpp::load_model(2, argv_cmap, 1).has_value());
    CHECK(xpp::client_session().colormap == 3);
    CHECK(xpp::load_model(2, argv_good, 1).has_value());
    CHECK(xpp::client_session().colormap == 0);

    /* a map is its own Model's: the next model loaded is not taken for
       one (its method is not made discrete) */
    const int method = xpp::client_session().numerics.method;
    CHECK(method != 0);
    /* the conversion of an earlier run is used as it stands: this test starts from none
       (an old one, from before an option was renamed, would no longer load) */
    xpp::files::remove("build/test_load_map.odex");
    const char map[] = "build/test_load_map.ode";
    {
        xpp::Writer w(map);
        CHECK(w && w.write("x(t+1)=x/2\ninit x=1\ndone\n") && w.commit());
    }
    char map_arg[] = "build/test_load_map.ode";
    char *argv_map[] = {arg0, map_arg, NULL};
    CHECK(xpp::load_model(2, argv_map, 1).has_value());
    CHECK(xpp::client_session().model().this_file == "build/test_load_map.odex");
    CHECK(xpp::client_session().numerics.method == 0);
    CHECK(xpp::load_model(2, argv_good, 1).has_value());
    CHECK(xpp::client_session().model().is_a_map == 0);
    CHECK(xpp::client_session().numerics.method == method);

    /* the nullclines, frozen or not, are the Session's: the next model
       loaded has none of them */
    {
        xpp::NullclineState &nc = xpp::client_session().nullcline_state;
        nc.frozen_started = true;
        nc.frozen.emplace_back();
        nc.frozen.back().nmx = 1;
        nc.frozen.back().xn.assign(4, 0.5f);
        nc.x_null.assign(4, 0.5f);
        nc.num_x_n = 1;
    }
    CHECK(xpp::load_model(2, argv_good, 1).has_value());
    CHECK(!xpp::client_session().nullcline_state.frozen_started);
    CHECK(xpp::client_session().nullcline_state.frozen.empty());
    CHECK(xpp::client_session().nullcline_state.num_x_n == 0);

    /* W214 (#245): a name that clashes with an array's member says so, in
       the name as its author wrote it, at the later declaration, in both
       readers */
    auto clash = [&](const char *path, const char *text) {
        {
            xpp::Writer w(path);
            CHECK(w && w.write(text) && w.commit());
        }
        std::string p = path;
        char *argv_clash[] = {arg0, p.data(), NULL};
        return xpp::load_model(2, argv_clash, 1);
    };
    for (const char *text : {"x[1..3]'=-x[j]\npar x2=5\ndone\n", "x[1..3]'=-x[j]\nx2=5\ndone\n"}) {
        d = clash("build/test_load_clash.ode", text);
        CHECK(!d.has_value());
        if (!d) {
            CHECK_STR(d.error().what.c_str(), "`x2` is already a member of the array x[1..3], declared at line 1");
            CHECK(d.error().place.line == 2);
        }
    }
    d = clash("build/test_load_clash.ode", "par X2=5\nx[1..3]'=-x[j]\ndone\n");
    CHECK(!d.has_value());
    if (!d) {
        CHECK_STR(d.error().what.c_str(), "`x2`, a member of the array x[1..3], is already declared at line 1");
        CHECK(d.error().place.line == 2);
    }
    d = clash("build/test_load_clash.ode", "Vm'=1\nvm'=2\ndone\n");
    CHECK(!d.has_value());
    if (!d) {
        CHECK_STR(d.error().what.c_str(), "Duplicate name vm (names match without case: Vm is declared at line 1)");
        CHECK(d.error().place.line == 2);
    }
    d = clash("build/test_load_clash.odex", "x[j]' = -x[j] for j in 1..3\npar x2 = 5\n");
    CHECK(!d.has_value());
    if (!d) {
        CHECK_STR(d.error().what.c_str(), "`x2` is already a member of the array x[1..3], declared at 1:1");
        CHECK(d.error().place.line == 2);
        CHECK(d.error().place.col == 5);
    }

    TEST_REPORT("load: build, then swap");
}
