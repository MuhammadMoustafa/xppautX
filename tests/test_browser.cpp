/* W526 (#590): the core functions behind the browser.* commands, which the data browser window's keys call too
   after their forms: find a value, the rows First..Last kept, the columns added, replaced (by a formula, a sequence,
   an integral, a derivative) and deleted, the data transposed, and the block of the table a client shows, each
   accepted and refused. A refusal changes nothing. The commands' own arguments are servercheck's data section. */
#include "load_args.h"
#include "xpptest.h"
#include "adj2.h"
#include "browse.h"
#include "integrate.h"
#include "session.h"
#include "xpp_batch.h"
#include <string>

int main()
{
    char name[] = "test_browser", path[] = "examples/ode/lecar.odex";
    char *argv[] = {name, path, nullptr};
    const xpp::Loaded loaded = load_args(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("browser");
    xpp::Session &s = **loaded;
    xpp::batch_start(s);
    xpp::BROWSER &view = s.browser.view;

    /* no data: nothing to find or keep */
    CHECK(!xpp::find_in_column(s, "V", 0));
    CHECK(!xpp::set_first_row(s, 0));
    CHECK(!xpp::restore_whole_data(s));
    const auto run = xpp::run_from(s, xpp::RunFrom::initial);
    CHECK(run.has_value());
    const int rows = s.data_store.rows;
    CHECK(rows == 601 && view.maxrow == rows && view.istart == 0 && view.iend == rows);

    /* find: the row, from the selected one on, nearest the value, which becomes the selected row */
    const double v100 = static_cast<double>(s.data_store.col[1][100]);
    const auto found = xpp::find_in_column(s, "v", v100);
    CHECK(found && *found == 100 && view.row0 == 100);
    CHECK(!xpp::find_in_column(s, "nosuch", 0) && view.row0 == 100);

    /* the rows kept: a row in the data, the first before the last, the last not before the first */
    CHECK(!xpp::set_first_row(s, -1) && !xpp::set_first_row(s, rows));
    const auto first = xpp::set_first_row(s, 10);
    CHECK(first && *first == 10 && view.istart == 10 && view.row0 == 10);
    CHECK(!xpp::set_last_row(s, 5) && view.iend == rows);
    const auto last = xpp::set_last_row(s, 50);
    CHECK(last && *last == 50 && view.iend == 51 && view.row0 == 50);
    CHECK(!xpp::set_first_row(s, 51) && view.istart == 10);
    const auto whole = xpp::restore_whole_data(s);
    CHECK(whole && *whole == rows - 1 && view.istart == 0 && view.iend == rows);

    /* an added column: named once, computed from the others, refused when its formula does not compile */
    const int columns = view.maxcol;
    CHECK(xpp::add_data_column(s, "vw", "v*w") && view.maxcol == columns + 1);
    CHECK(s.browser.added_columns.back().name == "VW");
    int vw = -1;
    xpp::find_variable(s, "vw", &vw);
    CHECK(vw == columns && static_cast<double>(s.data_store.col[vw][100]) == static_cast<double>(static_cast<float>(
                                static_cast<double>(s.data_store.col[1][100]) * static_cast<double>(s.data_store.col[2][100]))));
    CHECK(!xpp::add_data_column(s, "VW", "v") && !xpp::add_data_column(s, "", "v") && !xpp::add_data_column(s, "zz", "v*") && view.maxcol == columns + 1);

    /* replace: a sequence, an integral and a derivative; refused whole, and undone */
    const float old_vw = s.data_store.col[vw][7];
    CHECK(xpp::replace_data_column(s, "vw", "1:601"));
    CHECK(s.data_store.col[vw][0] == 1.0f && s.data_store.col[vw][600] == 601.0f);
    CHECK(xpp::unreplace_column(s) && s.data_store.col[vw][7] == old_vw);
    CHECK(!xpp::unreplace_column(s));
    CHECK(xpp::replace_data_column(s, "vw", "2;0.5") && s.data_store.col[vw][2] == 3.0f);
    CHECK(xpp::replace_data_column(s, "vw", "@v") && xpp::replace_data_column(s, "vw", "&v"));
    CHECK(!xpp::replace_data_column(s, "vw", "@nosuch") && !xpp::replace_data_column(s, "vw", "v*") && !xpp::replace_data_column(s, "vw", "3:3"));
    CHECK(!xpp::replace_data_column(s, "nosuch", "v") && !xpp::replace_data_column(s, "vw", "1:x"));
    CHECK(xpp::unreplace_column(s));

    /* deleting: only an added column, and not one a graph plots */
    CHECK(!xpp::delete_added_column(s, "v") && !xpp::delete_added_column(s, "nosuch"));
    CHECK(xpp::delete_added_column(s, "vw") && view.maxcol == columns && s.browser.added_columns.empty());
    CHECK(!xpp::delete_added_column(s, "vw"));

    /* the block of the table a client shows: fitted into the data, its rows as JSON */
    s.browser.block = {598, 5, 1, 2};
    xpp::fit_browser_block(s);
    const std::string block = xpp::browser_block_rows(s);
    CHECK(block.front() == '[' && block.back() == ']' && block.find("],[") != std::string::npos);
    CHECK(std::count(block.begin(), block.end(), '[') == 1 + 3); /* the three rows left: 598, 599, 600 */
    s.browser.block = {10000, 1, 99, 1};
    xpp::fit_browser_block(s);
    CHECK(s.browser.block.from == rows - 1 && s.browser.block.col == columns - 1);
    s.browser.block.count = 0;
    CHECK(xpp::browser_block_rows(s) == "[]");

    /* transposing: every column and row it reads is in the data, the settings are kept, nothing changed by a refusal */
    const int neq = s.model().neq;
    CHECK(!xpp::transpose_data(s, "nosuch", 2, 1, 0, 1, 1));
    CHECK(!xpp::transpose_data(s, "v", 0, 1, 0, 1, 1) && !xpp::transpose_data(s, "v", 2, 0, 0, 1, 1) && !xpp::transpose_data(s, "v", 2, 1, -1, 1, 1));
    CHECK(!xpp::transpose_data(s, "v", neq + 1, 1, 0, 1, 1)); /* past the last column */
    CHECK(!xpp::transpose_data(s, "v", 2, 1, 1, 2, rows)); /* past the last row */
    CHECK(!xpp::transpose_data(s, "v", 2, 1, 0, neq, 1)); /* more columns than the browser has */
    CHECK(!s.adjoint.transposed_here && view.data == s.data_store.col);
    CHECK(xpp::transpose_data(s, "v", 2, 1, 0, 1, 1));
    CHECK(s.adjoint.transposed_here && view.maxrow == 2 && s.adjoint.transpose.firstcol == "v" && s.adjoint.transpose.col0 == 2);
    CHECK(view.data[0][0] == 1.0f && view.data[0][1] == 2.0f && view.data[1][0] == s.data_store.col[1][0]);
    CHECK(!xpp::delete_added_column(s, "vw")); /* a transposed table is no stored data */

    TEST_REPORT("browser");
}
