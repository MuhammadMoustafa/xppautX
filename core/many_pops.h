#pragma once

#include "grobs.h"

namespace xpp {

/* The plot windows: every window's graph settings, the active one, and
   the windows a command draws on (all open ones under Simulplot). */
struct XppPlotWindows {
    std::vector<GRAPH> graph{1};   /* graph[i].Use: window i is open */
    GRAPH *current;        /* &graph[active] */
    int active;            /* the active window's index */
    int count;             /* how many are open */
    std::vector<int> open{0};      /* the open windows' indices, count of them */
    int simul;             /* Simulplot: draw on every open window */
    XppWinId draw_win;     /* graph[active].w, the window drawn into */
};

} // namespace xpp
