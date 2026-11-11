/* W613 (#681): the data store's columns are floats or doubles by the storage_precision setting, read and written through
   one accessor (Column). Checked: a column rounds on a write under single and keeps a double under double; the store
   converts in place, with no question, when the setting changes (single to double exactly, double to single as storing
   under single would have rounded it), its table and its added columns staying valid; a run under double holds the
   integrator's own values, which a single store holds rounded to floats. */
#include "load_args.h"
#include "xpptest.h"
#include "browse.h"
#include "integrate.h"
#include "session.h"
#include "storage.h"
#include "xpp_batch.h"
#include <cmath>
#include <vector>

namespace {

constexpr double fine = 0.1; /* not a float: 0.1 differs from its float by about 1.5e-9 */

double as_float(double v) { return static_cast<double>(static_cast<float>(v)); }

void check_column()
{
    std::vector<float> f(2);
    std::vector<double> d(2);
    const xpp::Column single(f.data()), wide(d.data());
    CHECK(!single.is_double() && wide.is_double());
    single[1] = fine;
    wide[1] = fine;
    CHECK(static_cast<double>(single[1]) == as_float(fine));
    CHECK(static_cast<double>(wide[1]) == fine);
    single[0] += 0.5;
    CHECK(f[0] == 0.5f);
    CHECK(single.values(0, 2).size() == 2 && single.as_floats(0, 2)[1] == static_cast<float>(fine));
    const std::vector<double> src{1.0, fine};
    wide.store(0, src);
    CHECK(d[0] == 1.0 && d[1] == fine);
}

void check_conversion()
{
    xpp::DataStore store;
    store.allocate(10, 3, xpp::StoragePrecision::single);
    CHECK(store.precision() == xpp::StoragePrecision::single && store.value_bytes() == sizeof(float));
    const xpp::ColumnTable table = store.col;
    store.add_column(3);
    for (int r = 0; r < 4; r++) {
        table[0][r] = r * fine;
        table[1][r] = 1.0 / 3.0 + r;
        table[3][r] = r - fine;
    }
    store.rows = 4;
    /* single to double: every value as it was held */
    CHECK(store.set_precision(xpp::StoragePrecision::double_));
    CHECK(store.precision() == xpp::StoragePrecision::double_ && store.col == table && table[1].is_double() && table[3].is_double());
    for (int r = 0; r < 4; r++) {
        CHECK(static_cast<double>(table[0][r]) == as_float(r * fine));
        CHECK(static_cast<double>(table[3][r]) == as_float(r - fine));
    }
    /* a double is kept whole from here, and back to single it rounds as storing under single does */
    table[1][2] = 1.0 / 3.0 + 2;
    CHECK(static_cast<double>(table[1][2]) == 1.0 / 3.0 + 2);
    CHECK(store.set_precision(xpp::StoragePrecision::single));
    CHECK(static_cast<double>(table[1][2]) == as_float(1.0 / 3.0 + 2));
    CHECK(!table[1].is_double() && !table[3].is_double());
    /* growing keeps the values at the precision, and thinning keeps every second row */
    CHECK(store.fit(3, 20) && store.max_rows == 20);
    CHECK(static_cast<double>(table[1][2]) == as_float(1.0 / 3.0 + 2));
    store.thin(3);
    CHECK(store.rows == 2 && static_cast<double>(table[0][1]) == as_float(2 * fine));
    /* a derived data set's own columns are made at the store's precision, and retyped with it */
    xpp::LentColumns lent;
    const xpp::ColumnTable shown = lent.make(store, 2, 5, 2);
    shown[0][1] = fine;
    CHECK(!shown[0].is_double() && !shown[2].is_double());
    CHECK(store.set_precision(xpp::StoragePrecision::double_));
    lent.retype(store);
    CHECK(shown[0].is_double() && shown[2].is_double() && static_cast<double>(shown[0][1]) == as_float(fine));
}

/* a lecar run under the precision: the stored rows of every column, and the state the integrator ended in */
std::vector<std::vector<double>> run_stored(xpp::StoragePrecision precision, std::vector<double> &final_state)
{
    char name[] = "test_storage_precision", path[] = "examples/ode/lecar.odex";
    char *argv[] = {name, path, nullptr};
    const xpp::Loaded loaded = load_args(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) return {};
    xpp::Session &s = **loaded;
    xpp::batch_start(s);
    s.numerics.storage_precision = precision;
    xpp::storage_precision_changed(s);
    CHECK(xpp::run_from(s, xpp::RunFrom::initial).has_value());
    std::vector<std::vector<double>> columns;
    for (int c = 0; c <= s.model().neq; c++) columns.push_back(s.data_store.col[c].values(0, s.data_store.rows));
    final_state = s.data_store.current;
    return columns;
}

void check_run()
{
    std::vector<double> state_single, state_double;
    const auto single = run_stored(xpp::StoragePrecision::single, state_single);
    const auto wide = run_stored(xpp::StoragePrecision::double_, state_double);
    CHECK(single.size() == wide.size() && single[0].size() == wide[0].size() && single[0].size() > 100);
    bool exact = true, rounded = true, differs = false;
    for (std::size_t c = 0; c < wide.size(); c++)
        for (std::size_t r = 0; r < wide[c].size(); r++) {
            rounded = rounded && single[c][r] == as_float(wide[c][r]);
            differs = differs || wide[c][r] != as_float(wide[c][r]);
        }
    CHECK(rounded); /* the single store is the double one, each value rounded to a float: XPPAUT's own */
    CHECK(differs); /* and the double store really holds more than a float */
    /* the last stored row is the integrator's own state, bit for bit, under double */
    const std::size_t last = wide[0].size() - 1;
    for (std::size_t c = 1; c < wide.size(); c++) exact = exact && wide[c][last] == state_double[c - 1];
    CHECK(exact);
}

} // namespace

TEST_CASE("test_storage_precision")
{
    check_column();
    check_conversion();
    check_run();
    TEST_REPORT("storage precision");
}
