#pragma once
/* The plot settings a Session holds (session.h) and a model's options give. */
#include "line_style.h"

#include <array>
#include <vector>

namespace xpp {

/* One curve of the starting graph: the columns it plots (0 the time, then the variables); a 2D curve ignores z. The
   defaults are the time, and the first variable twice. */
struct PlotCurve {
  int x = 0, y = 1, z = 1;
};

/* The main plot's settings in use (the model's `plot: { curves }` and axes options, the Viewaxes
   dialogs), a Session's (session.h): the curves the starting graph draws (the first holds the axes'
   variables), the axes' kind, the limits: a 2D plot's window or a 3D plot's box */
struct PlotSettings {
  std::vector<PlotCurve> curves{PlotCurve{}};
  int axes = 0, timplot = 0, plot_3d = 0;
  std::array<double,2> x_3d{}, y_3d{}, z_3d{};
  LineStyle start_line_style; /* the style every new curve starts with (@ lt=, line.draw, ...) */
};

} // namespace xpp
