/* W525 (#589): the core functions behind the analysis.* and table.* commands, which the Stochastic, Poincare
   map, Phase space, Color code and Tables menus call too: each accepted and refused, a refusal changing nothing.
   The commands' own arguments are servercheck's registry section. */
#include "load_args.h"
#include "xpptest.h"
#include "adj2.h"
#include "browse.h"
#include "histogram.h"
#include "integrate.h"
#include "markov.h"
#include "numerics.h"
#include "session.h"
#include "tabular.h"
#include "torus.h"
#include "xpp_batch.h"
#include <cmath>
#include <string>

int main()
{
    char name[] = "test_analysis", path[] = "examples/ode/lecar.odex";
    char *argv[] = {name, path, nullptr};
    const xpp::Loaded loaded = load_args(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("analysis");
    xpp::Session &s = **loaded;
    xpp::batch_start(s);

    /* the seed: a whole number of at least 0, and the same seed gives the same numbers */
    CHECK(xpp::set_random_seed(s, 7) && s.numerics.rand_seed == 7);
    const double first = s.random.uniform();
    CHECK(xpp::set_random_seed(s, 7) && s.random.uniform() == first);
    CHECK(!xpp::set_random_seed(s, -1) && s.numerics.rand_seed == 7);

    /* the analyses of no data: nothing to analyse, the histogram needs a row and a spectrum two */
    CHECK(!xpp::compute_hist(s));
    CHECK(!xpp::column_mean(s));
    CHECK(!xpp::mean_back(s) && !xpp::variance_back(s));
    CHECK(!xpp::load_adjoint(s) && !xpp::load_interaction(s) && !xpp::reload_histogram(s));
    CHECK(!xpp::new_adjoint(s));

    CHECK(xpp::run_from(s, xpp::RunFrom::initial));
    const int v = variable_column(s, "v");
    CHECK(v == 1);

    /* statistics: the variance of constant data is 0, never below it */
    s.histogram.info.col = v;
    const auto stats = xpp::column_mean(s);
    CHECK(stats && stats->variance >= 0);
    s.histogram.info.col = 0; /* time: uniform on the run, variance total^2/12 to a good approximation */
    const auto time_stats = xpp::column_mean(s);
    CHECK(time_stats && std::fabs(time_stats->mean - s.numerics.total_time / 2) < 1e-3 * s.numerics.total_time);

    /* a histogram counts every row in its bins; its range and bins are refused as the options refuse them, and a
       condition that is no formula makes nothing */
    xpp::HIST_INFO &info = s.histogram.info;
    info.col = v;
    info.nbins = 10;
    info.xlo = -100;
    info.xhi = 100;
    info.cond = "";
    const auto counted = xpp::compute_hist(s);
    CHECK(counted && counted->bins == 10 && counted->samples == s.data_store.rows);
    CHECK(xpp::reload_histogram(s));
    info.cond = "t<";
    const int hist_len = s.histogram.hist_len;
    CHECK(!xpp::compute_hist(s) && s.histogram.hist_len == hist_len);
    info.cond = "t<1";
    const auto some = xpp::compute_hist(s);
    CHECK(some && some->samples > 0 && some->samples < s.data_store.rows);
    info.cond = "";
    info.nbins = 1;
    CHECK(!xpp::compute_hist(s));
    info.nbins = 10;
    info.xlo = 1;
    info.xhi = -1;
    CHECK(!xpp::compute_hist(s));

    /* the 2D histogram needs bins that fit the data table's rows (XPPAUT wrote past its columns) */
    xpp::data_back(s);
    info.xlo = -100;
    info.xhi = 100;
    info.ylo = -100;
    info.yhi = 100;
    info.col2 = 2;
    info.nbins = info.nbins2 = 4096;
    CHECK(!xpp::new_2d_hist(s));
    info.nbins = info.nbins2 = 5;
    CHECK(xpp::new_2d_hist(s) && s.histogram.hist_len == 25);

    /* a spectrum's window is 2 points or more and no more than the rows (XPPAUT divided by zero at 1) */
    xpp::data_back(s);
    s.histogram.spec_col = v;
    s.histogram.spec_win = 2;
    s.histogram.spec_type = 0;
    s.histogram.spec_wid = 1;
    CHECK(!xpp::compute_sd(s));
    s.histogram.spec_wid = s.data_store.rows + 1;
    CHECK(!xpp::compute_sd(s));
    s.histogram.spec_wid = 64;
    const auto points = xpp::compute_sd(s);
    CHECK(points && *points == 32);
    s.histogram.spec_win = 5;
    CHECK(!xpp::compute_sd(s));
    s.histogram.spec_win = 2;
    CHECK(!xpp::just_sd(s, 3)); /* a type that is none of the three */

    /* the correlation's lags: 2 up to half the data less 2 */
    xpp::data_back(s);
    info.col = v;
    info.col2 = 2;
    info.fftc = 0;
    info.nbins = 1;
    CHECK(!xpp::compute_correl(s));
    info.nbins = s.data_store.rows;
    CHECK(!xpp::compute_correl(s));
    info.nbins = 10;
    CHECK(xpp::compute_correl(s).has_value());
    info.fftc = 2;
    CHECK(!xpp::compute_correl(s));

    /* the torus: all, chosen or none at a period above 0; a refusal changes nothing */
    xpp::data_back(s);
    CHECK(xpp::set_torus(s, xpp::TorusMode::all, 6.28, {}) && s.numerics.torus && s.numerics.torus_period == 6.28 && s.itor[0] == 1 && s.itor[1] == 1);
    CHECK(xpp::set_torus(s, xpp::TorusMode::none, {}, {}) && !s.numerics.torus && s.itor[0] == 0);
    CHECK(xpp::set_torus(s, xpp::TorusMode::choose, {}, std::vector<int>{1}) && s.numerics.torus && s.itor[0] == 0 && s.itor[1] == 1 && s.numerics.torus_period == 6.28);
    CHECK(!xpp::set_torus(s, xpp::TorusMode::all, 0.0, {}) && s.itor[0] == 0 && s.numerics.torus_period == 6.28);
    CHECK(xpp::set_torus(s, xpp::TorusMode::choose, {}, std::vector<int>{}) && !s.numerics.torus);
    CHECK(xpp::set_torus(s, xpp::TorusMode::none, {}, {}) && !s.numerics.torus);

    /* the Poincare map: its settings set as given and the others kept; none leaves them */
    CHECK(xpp::set_poincare_map(s, xpp::PoincareMap::section, 2, 0.25, xpp::SectionSign::positive, true));
    CHECK(s.numerics.poincare_map == xpp::PoincareMap::section && s.numerics.poincare_variable == 2 && s.numerics.poincare_plane == 0.25 &&
          s.numerics.poincare_sign == xpp::SectionSign::positive && s.numerics.poincare_stop);
    CHECK(xpp::set_poincare_map(s, xpp::PoincareMap::period, {}, {}, {}, false) && s.numerics.poincare_map == xpp::PoincareMap::period &&
          s.numerics.poincare_variable == 2 && !s.numerics.poincare_stop);
    CHECK(!xpp::set_poincare_map(s, xpp::PoincareMap::max, -1, {}, {}, {}) && s.numerics.poincare_map == xpp::PoincareMap::period);
    CHECK(xpp::set_poincare_map(s, xpp::PoincareMap::none, {}, {}, {}, {}) && s.numerics.poincare_map == xpp::PoincareMap::none);

    /* the color coding of the active graph: a range of limits that is empty is refused, velocity needs a trajectory */
    GRAPH &g = *s.plot_windows.current;
    CHECK(xpp::set_color_coding(s, xpp::ColorBy::variable, 2, std::pair{0.0, 2.0}) && g.ColorFlag == 2 && g.ColorValue == 2 && g.min_scale == 0 && g.color_scale == 2);
    CHECK(!xpp::set_color_coding(s, xpp::ColorBy::variable, 2, std::pair{1.0, 1.0}) && g.color_scale == 2);
    CHECK(!xpp::set_color_coding(s, xpp::ColorBy::velocity, 0, std::pair{-1.0, 1.0}) && g.ColorFlag == 2);
    CHECK(!xpp::set_color_coding(s, xpp::ColorBy::variable, -1, {}));
    CHECK(xpp::set_color_coding(s, xpp::ColorBy::variable, 2, {}) && g.color_scale > 0);
    CHECK(xpp::set_color_coding(s, xpp::ColorBy::velocity, 0, {}) && g.ColorFlag == 1);
    CHECK(xpp::set_color_coding(s, xpp::ColorBy::none, 0, {}) && g.ColorFlag == 0);

    /* the adjoint's settings */
    CHECK(xpp::set_adjoint_parameters(s, 30, 1e-4) && s.adjoint.maxit == 30 && s.adjoint.err == 1e-4);
    CHECK(!xpp::set_adjoint_parameters(s, 0, 1e-4) && !xpp::set_adjoint_parameters(s, 30, 0) && s.adjoint.maxit == 30 && s.adjoint.err == 1e-4);

    /* the lookup tables: a table of a formula and one of a file, found by name in any case */
    char tables_name[] = "test_analysis", tables_path[] = "tools/models/fftcon_test.odex";
    char *tables_argv[] = {tables_name, tables_path, nullptr};
    const xpp::Loaded other = load_args(2, tables_argv, 1);
    CHECK(other.has_value());
    if (other) {
        xpp::Session &t = **other;
        xpp::batch_start(t);
        const auto wgt = xpp::find_table(t, "WGT");
        CHECK(wgt.has_value() && !xpp::find_table(t, "nosuch") && !xpp::find_table(t, "k"));
        if (wgt) {
            xpp::TableEdit edit;
            edit.points = 9;
            edit.x_min = 0;
            edit.x_max = 8;
            edit.formula = "2*t";
            const auto points = xpp::edit_table(t, *wgt, edit);
            CHECK(points && *points == 9 && t.tables[static_cast<std::size_t>(*wgt)].n == 9 && t.tables[static_cast<std::size_t>(*wgt)].y[4] == 8);
            edit.file = "x.tab";
            CHECK(!xpp::edit_table(t, *wgt, edit)); /* a table of a formula is not read from a file */
            edit.file.reset();
            edit.points = 1;
            CHECK(!xpp::edit_table(t, *wgt, edit) && t.tables[static_cast<std::size_t>(*wgt)].n == 9);
            const auto shown = xpp::view_table(t, *wgt);
            CHECK(shown && *shown == 9 && t.data_store.col[1][4] == 8.0f);
        }
    }
    TEST_REPORT("analysis");
}
