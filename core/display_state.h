#pragma once
/* What the page displays, held by the core (W65, docs/ui-v2.md "Display
   state"): each plot window's earlier runs until Erase, the zoom shown and
   whether the earlier runs are drawn.
   Members of xpp::Session (session.h), set by the `display` and `autox`
   commands or by the runs themselves and sent as data (plot_data.cpp's
   `runs` and `plots` events, json_autox.cpp's `autoxview`), so a session
   file can save them. C++ only. */
#include <array>
#include <vector>

#include "struct.h"
#include "zoom.h"
#include "storage.h"
#include "colormap.h"
#include "line_style.h"

namespace xpp {

/* the curves a window plots: its series' identity */
struct PlotCurves {
    int nvars, three;
    std::vector<int> xv, yv, zv;
    std::vector<LineStyle> style;
    std::vector<Color> color;
    std::array<int, plot_axes> shift{};
    bool operator==(const PlotCurves &) const = default;
};

/* one run's stored columns, as the page has them: the columns `cols` (T
   first), each `rows` values */
struct PlotRun {
    PlotCurves curves{};
    int rows = 0;
    std::vector<int> cols;
    std::vector<OwnedColumn> data; /* at the store's precision when the run was kept */
};

/* a plot window's display state */
struct PlotDisplay {
    Zoom zoom;
    bool show_runs = true; /* the legend's "previous runs" toggle */
    bool freeze_runs = true; /* retain replaced trajectories until Erase */
    /* earlier runs, oldest first; Erase (and Redraw) forgets them */
    std::vector<PlotRun> runs;
    bool erased = false; /* Erase blanked the window: the current run is not drawn */
    bool live = false;   /* the current run grew by appends in this command */
    /* the current run as the page has it (to become an earlier run) */
    bool has_cur = false;
    PlotRun cur;
    unsigned long cur_version = 0;
    /* the axes and curves the zoom was made for: other ones drop it */
    bool axes_seen = false;
    std::array<double, 4> axes{};
    PlotCurves axes_curves{};
};

} // namespace xpp
