/* Unit tests for tabular lookups and table loading (tabular.h, W373):
   xyvals tables checked for strictly increasing x at load time (all or nothing,
   naming file and line), and interval search by halves (std::lower_bound). */
#include "xpptest.h"
#include "tabular.h"
#include "model.h"
#include "session.h"
#include "xpp_files.h"
#include "xpp_io.h"

#include <cmath>
#include <string>

namespace {

struct ScratchDir {
    std::string path;
    bool made;
    ScratchDir()
    {
        path = xpp::files::make_temp_dir();
        made = !path.empty();
        if (!made) path = ".";
    }
    ~ScratchDir()
    {
        if (made) xpp::files::remove_temp_dir(path.c_str());
    }
};

const std::string &scratch_dir()
{
    static const ScratchDir dir;
    return dir.path;
}

std::string scratch(const char *name) { return scratch_dir() + "/" + name; }

bool write_file(const std::string &path, const std::string &content)
{
    xpp::Writer w(path.c_str());
    return w && w.write(content) && w.commit();
}

bool close_to(double a, double b)
{
    return std::fabs(a - b) < 1e-9;
}

}

int main(void)
{
    xpp::Session &s = xpp::client_session();
    xpp::init_table(s);

    const std::string good_tab = scratch("good_xy.tab");
    CHECK(write_file(good_tab,
        "xyvals\n"
        "4\n"
        "1.0 10.0\n"
        "2.0 20.0\n"
        "3.0 30.0\n"
        "5.0 50.0\n"));

    auto res = xpp::load_table(s, good_tab, 0, 0);
    CHECK(res.has_value());
    CHECK(s.tables[0].xyvals == 1);
    CHECK(s.tables[0].n == 4);
    CHECK(close_to(s.tables[0].xlo, 1.0));
    CHECK(close_to(s.tables[0].xhi, 5.0));

    /* Lookups at the points */
    CHECK(close_to(xpp::lookup(s, 1.0, 0), 10.0));
    CHECK(close_to(xpp::lookup(s, 2.0, 0), 20.0));
    CHECK(close_to(xpp::lookup(s, 3.0, 0), 30.0));
    CHECK(close_to(xpp::lookup(s, 5.0, 0), 50.0));

    /* Lookups between points */
    CHECK(close_to(xpp::lookup(s, 1.5, 0), 15.0));
    CHECK(close_to(xpp::lookup_slope(s, 1.5, 0), 10.0));
    CHECK(close_to(xpp::lookup(s, 2.5, 0), 25.0));
    CHECK(close_to(xpp::lookup_slope(s, 2.5, 0), 10.0));
    CHECK(close_to(xpp::lookup(s, 4.0, 0), 40.0));
    CHECK(close_to(xpp::lookup_slope(s, 4.0, 0), 10.0));

    /* Lookups outside points (extrapolations) */
    /* Before first point: linear extrapolation using first segment (slope 10) */
    CHECK(close_to(xpp::lookup(s, 0.0, 0), 0.0));
    CHECK(close_to(xpp::lookup_slope(s, 0.0, 0), 10.0));
    CHECK(close_to(xpp::lookup(s, -1.0, 0), -10.0));

    /* Past last point: XPPAUT reverses the slope of the last segment (slope -10) */
    CHECK(close_to(xpp::lookup(s, 6.0, 0), 40.0));
    CHECK(close_to(xpp::lookup_slope(s, 6.0, 0), -10.0));
    CHECK(close_to(xpp::lookup(s, 7.0, 0), 30.0));

    /* Non-increasing x is refused with file and line */
    const std::string bad_dec = scratch("bad_dec.tab");
    CHECK(write_file(bad_dec,
        "xyvals\n"
        "3\n"
        "1.0 10.0\n"
        "0.5 20.0\n"
        "3.0 30.0\n"));

    auto bad_res1 = xpp::load_table(s, bad_dec, 1, 0);
    CHECK(!bad_res1.has_value());
    CHECK(bad_res1.error().where == "table");
    CHECK(bad_res1.error().what.find("strictly increase") != std::string::npos);
    CHECK(bad_res1.error().place.line == 4);
    CHECK(bad_res1.error().place.file == bad_dec);

    /* Duplicate x is refused with file and line */
    const std::string bad_dup = scratch("bad_dup.tab");
    CHECK(write_file(bad_dup,
        "xyvals\n"
        "3\n"
        "1.0 10.0\n"
        "1.0 20.0\n"
        "3.0 30.0\n"));

    auto bad_res2 = xpp::load_table(s, bad_dup, 1, 0);
    CHECK(!bad_res2.has_value());
    CHECK(bad_res2.error().where == "table");
    CHECK(bad_res2.error().what.find("strictly increase") != std::string::npos);
    CHECK(bad_res2.error().place.line == 4);
    CHECK(bad_res2.error().place.file == bad_dup);

    /* All-or-nothing: reloading an existing table with bad file leaves original intact */
    auto reload_bad = xpp::load_table(s, bad_dec, 0, 0);
    CHECK(!reload_bad.has_value());
    CHECK(s.tables[0].xyvals == 1);
    CHECK(s.tables[0].n == 4);
    CHECK(close_to(xpp::lookup(s, 1.5, 0), 15.0));
    CHECK(close_to(xpp::lookup(s, 4.0, 0), 40.0));

    TEST_REPORT("test_tabular");
}
