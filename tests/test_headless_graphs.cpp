/* W348 (#409): graph and equilibrium data with the headless UI defaults,
   before a protocol emitter is installed (the --silent load path). */
#include "xpptest.h"
#include "xpp_batch.h"
#include "graphics.h"
#include "marks_data.h"
#include "session.h"

int main()
{
    char name[] = "test_headless_graphs";
    char path[] = "tools/models/multiwin.odex";
    char *argv[] = {name, path, nullptr};
    const xpp::Loaded loaded = xpp::load_model(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("headless graphs");
    xpp::Session &s = **loaded;
    xpp::batch_start(s);
    CHECK(s.plot_windows.count == 3 && s.plot_windows.active == 0);
    for (int i = 0; i < 3; i++) {
        CHECK(s.plot_windows.graph[i].Use == 1);
        CHECK(s.plot_windows.graph[i].w == static_cast<XppWinId>(i + 1));
        CHECK(s.plot_windows.open[i] == i);
    }
    const GRAPH &g = s.plot_windows.graph[1];
    CHECK(g.xv[0] == 2 && g.yv[0] == 1);
    CHECK(g.xlo == -2 && g.xhi == 3 && g.ylo == -4 && g.yhi == 5);
    CHECK(s.plot_windows.graph[2].xv[0] == 0 && s.plot_windows.graph[2].yv[0] == 2);
    CHECK(s.plot_windows.current == &s.plot_windows.graph[0] && s.plot_windows.draw_win == 1);
    double equilibrium[] = {0, 0};
    eq_symb(s, equilibrium, 3);
    const auto &eqs = s.marks_shown.windows[0].eqs;
    CHECK(eqs.size() == 1);
    if (!eqs.empty()) CHECK(eqs[0].x == 0 && eqs[0].y == 0 && eqs[0].symbol == 3);
    eq_symb(s, equilibrium, 3);
    CHECK(eqs.size() == 1);
    marks_data_cleared(s, 0);
    CHECK(eqs.empty());
    TEST_REPORT("headless graphs");
}
