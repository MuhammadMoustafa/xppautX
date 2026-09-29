#ifndef XPP_DISPLAY_STATE_H
#define XPP_DISPLAY_STATE_H
/* What the page displays, held by the core (W65, docs/ui-v2.md "Display
   state"): each plot window's earlier runs until Erase, the zoom shown and
   whether the earlier runs are drawn; AUTO's hidden branches and zoom.
   Members of xpp::Session (session.h), set by the `display` and `auto`
   commands or by the runs themselves and sent as data (plot_data.cpp's
   `runs` and `plots` events, json_auto.cpp's `autoview`), so a session
   file can save them. C++ only. */
#include <array>
#include <string>
#include <vector>

#include "struct.h"

namespace xpp {

/* one axis' visible range; unset: the window's own */
struct AxisRange {
    bool set = false;
    double lo = 0, hi = 0;
    bool operator==(const AxisRange &) const = default;
};

/* the zoom shown: the part of a window's (or AUTO's) axes the page displays */
struct Zoom {
    AxisRange x, y;
    bool any() const { return x.set || y.set; }
    bool operator==(const Zoom &) const = default;
};

/* the curves a window plots: its series' identity */
struct PlotCurves {
    int nvars, three;
    int xv[MAXPERPLOT], yv[MAXPERPLOT], zv[MAXPERPLOT], line[MAXPERPLOT], color[MAXPERPLOT];
    int shift[3];
    bool operator==(const PlotCurves &) const = default;
};

/* one run's stored columns, as the page has them: the columns `cols` (T
   first), each `rows` values */
struct PlotRun {
    PlotCurves curves{};
    int rows = 0;
    std::vector<int> cols;
    std::vector<std::vector<float>> data;
};

/* a plot window's display state */
struct PlotDisplay {
    Zoom zoom;
    bool show_runs = true; /* the legend's "previous runs" toggle */
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

/* AUTO's diagram display: the points before `earlier` are the branches
   computed before Clear, hidden unless `show_earlier` */
struct AutoView {
    int earlier = 0;
    bool show_earlier = false;
    Zoom zoom;
    /* the axes the zoom was made for */
    bool axes_seen = false;
    std::array<double, 4> axes{};
};

} // namespace xpp
#endif
